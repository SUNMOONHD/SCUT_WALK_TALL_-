#include "path_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr double kZoneSwitchPenalty = 2.0;
constexpr double kMaxTime = 300.0;
constexpr double kMaxDistance = 200.0;
constexpr double kHeuristicWeight = 0.8; // A* 启发式权重（小于1.0使其更保守）

double nodeTypeSpeedLimit(const std::string &type)
{
	if (type == "escalator") return 0.65;
	if (type == "stairs") return 0.5;
	return 1.2;
}

struct QueueItem {
	double fCost = 0.0;  // f(n) = g(n) + h(n)
	double gCost = 0.0;  // 实际代价
	std::string node;
	std::string prevType;
	int zoneSwitches = 0;

	bool operator>(const QueueItem &other) const
	{
		return fCost > other.fCost;
	}
};

// A* 启发式函数：欧几里得距离
double heuristic(
	const std::string &current,
	const std::string &target,
	const std::unordered_map<std::string, StationNode> &nodes)
{
	auto currentIt = nodes.find(current);
	auto targetIt = nodes.find(target);
	if (currentIt != nodes.end() && targetIt != nodes.end()) {
		double dx = currentIt->second.x - targetIt->second.x;
		double dy = currentIt->second.y - targetIt->second.y;
		// 考虑楼层差异的3D距离
		double dz = static_cast<double>(currentIt->second.floor - targetIt->second.floor) * 5.0;
		return kHeuristicWeight * std::sqrt(dx*dx + dy*dy + dz*dz);
	}
	return 0.0;
}

double edgeTimeCost(const GraphEdge &edge, const std::string &toNodeType = "")
{
	if (edge.transferTime > 0.0) {
		return edge.transferTime;
	}
	if (edge.length > 0.0) {
		double speed = toNodeType.empty() ? 1.2 : nodeTypeSpeedLimit(toNodeType);
		return edge.length / speed;
	}
	return 1.0;
}

double edgeCongestionCost(
	const GraphEdge &edge,
	int edgeIndex,
	const std::unordered_map<std::string, int> &edgeOccupancy)
{
	auto it = edgeOccupancy.find(std::to_string(edgeIndex));
	int occupancy = (it != edgeOccupancy.end()) ? it->second : 0;
	if (edge.capacity > 0.0) {
		return static_cast<double>(occupancy) / edge.capacity;
	}
	return 0.0;
}

double nodeCongestionCost(
	const std::string &nodeId,
	const std::unordered_map<std::string, int> &nodeOccupancy,
	double nodeCapacity = 10.0)
{
	auto it = nodeOccupancy.find(nodeId);
	int occupancy = (it != nodeOccupancy.end()) ? it->second : 0;
	if (nodeCapacity > 0.0) {
		return static_cast<double>(occupancy) / nodeCapacity;
	}
	return 0.0;
}

double computeEdgeCost(
	const GraphEdge &edge,
	int edgeIndex,
	PathObjective objective,
	const PathWeights &weights,
	const std::unordered_map<std::string, int> &edgeOccupancy,
	const std::unordered_map<std::string, int> &nodeOccupancy,
	const std::string &currentType,
	const std::string &nextType,
	const std::string &nextNodeId,
	double nextNodeCapacity,
	int &zoneSwitches)
{
	double t = edgeTimeCost(edge, nextNodeId.empty() ? "" : nextType);
	double d = edge.length;
	double c = edgeCongestionCost(edge, edgeIndex, edgeOccupancy);
	double n = nodeCongestionCost(nextNodeId, nodeOccupancy, nextNodeCapacity);

	int zSwitch = 0;
	if (!currentType.empty() && !nextType.empty() && currentType != nextType) {
		zSwitch = 1;
	}
	zoneSwitches += zSwitch;

	double congestionFactor = (c + n * 0.5) * 0.5;

	switch (objective) {
		case PathObjective::MinTime:
			return t;
		case PathObjective::MinDistance:
			return d;
		case PathObjective::MinCongestion:
			return congestionFactor * 10.0;
		case PathObjective::MinZoneSwitches:
			return (zSwitch > 0) ? kZoneSwitchPenalty + t * 0.1 : t * 0.1;
		case PathObjective::WeightedSum:
			return weights.wTime * (t / kMaxTime)
				 + weights.wDistance * (d / kMaxDistance)
				 + weights.wCongestion * congestionFactor
				 + weights.wZoneSwitch * (zSwitch > 0 ? 1.0 : 0.0);
	}
	return t;
}

std::vector<std::string> reconstructPath(
	const std::unordered_map<std::string, std::string> &previous,
	const std::string &from,
	const std::string &to)
{
	std::vector<std::string> path;
	for (std::string cursor = to; !cursor.empty();) {
		path.push_back(cursor);
		if (cursor == from) {
			break;
		}
		auto it = previous.find(cursor);
		if (it == previous.end()) {
			return {};
		}
		cursor = it->second;
	}
	std::reverse(path.begin(), path.end());
	if (path.empty() || path.front() != from || path.back() != to) {
		return {};
	}
	return path;
}

} // namespace

std::vector<std::string> PathPlanner::findPath(
	const MetroGraph &graph,
	const std::string &from,
	const std::string &to,
	PathObjective objective,
	const PathWeights &weights,
	const std::unordered_map<std::string, int> &edgeOccupancy,
	const std::unordered_map<std::string, int> &nodeOccupancy) const
{
	if (from.empty() || to.empty()) {
		return {};
	}

	if (from == to) {
		return {from};
	}

	const auto &nodes = graph.nodes();
	if (nodes.empty()) {
		return {};
	}
	if (nodes.find(from) == nodes.end() || nodes.find(to) == nodes.end()) {
		return {};
	}
	
	// 检查缓存（不考虑拥堵状态的静态路径缓存）
	if (cacheEnabled_ && objective != PathObjective::MinCongestion && objective != PathObjective::WeightedSum) {
		std::string cacheKey = from + "|" + to + "|" + std::to_string(static_cast<int>(objective));
		auto cacheIt = pathCache_.find(cacheKey);
		if (cacheIt != pathCache_.end()) {
			return cacheIt->second;
		}
	}

	const auto &edges = graph.edges();
	const double inf = std::numeric_limits<double>::infinity();
	std::unordered_map<std::string, double> gCost;  // A*: 实际代价
	std::unordered_map<std::string, std::string> previous;
	std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;

	for (const auto &entry : nodes) {
		gCost.emplace(entry.first, inf);
	}
	gCost[from] = 0.0;

	std::string startType;
	auto startIt = nodes.find(from);
	if (startIt != nodes.end()) {
		startType = startIt->second.type;
	}
	
	// A*: 初始 fCost = gCost + heuristic (Dijkstra 模式时启发式为0)
	double startHeuristic = useAStar_ ? heuristic(from, to, nodes) : 0.0;
	queue.push({startHeuristic, 0.0, from, startType, 0});

	while (!queue.empty()) {
		const QueueItem current = queue.top();
		queue.pop();

		// A*: 使用 gCost 进行松弛判断
		if (current.gCost > gCost[current.node]) {
			continue;
		}

		if (current.node == to) {
			break;
		}

		const auto adjacencyIt = graph.adjacency().find(current.node);
		if (adjacencyIt == graph.adjacency().end()) {
			continue;
		}

		for (std::size_t edgeIndex : adjacencyIt->second) {
			if (edgeIndex >= edges.size()) {
				continue;
			}

			const auto &edge = edges[edgeIndex];
			std::string nextNode;
			if (edge.from == current.node) {
				nextNode = edge.to;
			} else if (edge.bidirectional && edge.to == current.node) {
				nextNode = edge.from;
			} else {
				continue;
			}

			std::string nextType;
			auto nextIt = nodes.find(nextNode);
			double nextCapacity = 10.0;
			if (nextIt != nodes.end()) {
				nextType = nextIt->second.type;
				nextCapacity = nextIt->second.capacity > 0.0 ? nextIt->second.capacity : 10.0;
			}

			int zoneSwitches = current.zoneSwitches;
			const double edgeCost = computeEdgeCost(
				edge, static_cast<int>(edgeIndex), objective, weights,
				edgeOccupancy, nodeOccupancy, current.prevType, nextType, nextNode, nextCapacity, zoneSwitches);

			// A*: 计算新的 gCost 和 fCost (Dijkstra 模式时启发式为0)
			const double newGCost = current.gCost + edgeCost;
			if (newGCost < gCost[nextNode]) {
				gCost[nextNode] = newGCost;
				previous[nextNode] = current.node;
				double heuristicValue = useAStar_ ? heuristic(nextNode, to, nodes) : 0.0;
				double newFCost = newGCost + heuristicValue;
				queue.push({newFCost, newGCost, nextNode, nextType, zoneSwitches});
			}
		}
	}

	if (previous.find(to) == previous.end() && from != to) {
		return {};
	}

	std::vector<std::string> path = reconstructPath(previous, from, to);
	
	// 将结果存入缓存
	if (cacheEnabled_ && !path.empty() && objective != PathObjective::MinCongestion && objective != PathObjective::WeightedSum) {
		std::string cacheKey = from + "|" + to + "|" + std::to_string(static_cast<int>(objective));
		pathCache_[cacheKey] = path;
	}
	
	return path;
}

PathMetrics PathPlanner::computePathMetrics(
	const MetroGraph &graph,
	const std::vector<std::string> &path,
	const std::unordered_map<std::string, int> &edgeOccupancy,
	const std::unordered_map<std::string, int> &nodeOccupancy) const
{
	PathMetrics metrics;
	if (path.size() < 2) {
		return metrics;
	}

	const auto &nodes = graph.nodes();
	const auto &edges = graph.edges();
	std::string prevType;

	for (std::size_t i = 0; i + 1 < path.size(); ++i) {
		const std::string &from = path[i];
		const std::string &to = path[i + 1];

		auto fromIt = nodes.find(from);
		auto toIt = nodes.find(to);
		if (fromIt == nodes.end() || toIt == nodes.end()) {
			continue;
		}

		if (!prevType.empty() && fromIt->second.type != prevType) {
			metrics.zoneSwitches++;
		}
		prevType = fromIt->second.type;

		int foundEdgeIndex = -1;
		for (std::size_t ei = 0; ei < edges.size(); ++ei) {
			const auto &e = edges[ei];
			if ((e.from == from && e.to == to) || (e.bidirectional && e.from == to && e.to == from)) {
				foundEdgeIndex = static_cast<int>(ei);
				break;
			}
		}

		if (foundEdgeIndex < 0) {
			continue;
		}

		const auto &edge = edges[foundEdgeIndex];
		metrics.totalTime += edgeTimeCost(edge, toIt->second.type);
		metrics.totalDistance += edge.length;
		metrics.avgCongestion += edgeCongestionCost(edge, foundEdgeIndex, edgeOccupancy);
	}

	if (path.size() > 1) {
		metrics.avgCongestion /= static_cast<double>(path.size() - 1);
	}

	return metrics;
}

std::vector<std::vector<std::string>> PathPlanner::findParetoFrontier(
	const MetroGraph &graph,
	const std::string &from,
	const std::string &to,
	const std::unordered_map<std::string, int> &edgeOccupancy,
	const std::unordered_map<std::string, int> &nodeOccupancy) const
{
	std::vector<std::vector<std::string>> candidates;

	auto pathTime = findPath(graph, from, to, PathObjective::MinTime, {}, edgeOccupancy, nodeOccupancy);
	if (!pathTime.empty()) candidates.push_back(pathTime);

	auto pathDist = findPath(graph, from, to, PathObjective::MinDistance, {}, edgeOccupancy, nodeOccupancy);
	if (!pathDist.empty() && pathDist != pathTime) candidates.push_back(pathDist);

	auto pathCong = findPath(graph, from, to, PathObjective::MinCongestion, {}, edgeOccupancy, nodeOccupancy);
	if (!pathCong.empty()) candidates.push_back(pathCong);

	auto pathZone = findPath(graph, from, to, PathObjective::MinZoneSwitches, {}, edgeOccupancy, nodeOccupancy);
	if (!pathZone.empty()) candidates.push_back(pathZone);

	if (candidates.size() <= 1) {
		return candidates;
	}

	std::vector<PathMetrics> metricsList;
	metricsList.reserve(candidates.size());
	for (const auto &p : candidates) {
		metricsList.push_back(computePathMetrics(graph, p, edgeOccupancy, nodeOccupancy));
	}

	std::vector<bool> dominated(candidates.size(), false);
	for (std::size_t i = 0; i < candidates.size(); ++i) {
		for (std::size_t j = 0; j < candidates.size(); ++j) {
			if (i == j) continue;
			const auto &a = metricsList[i];
			const auto &b = metricsList[j];
			bool aDomB = (a.totalTime <= b.totalTime)
				&& (a.totalDistance <= b.totalDistance)
				&& (a.avgCongestion <= b.avgCongestion)
				&& (a.zoneSwitches <= b.zoneSwitches);
			bool aStrict = (a.totalTime < b.totalTime)
				|| (a.totalDistance < b.totalDistance)
				|| (a.avgCongestion < b.avgCongestion)
				|| (a.zoneSwitches < b.zoneSwitches);
			if (aDomB && aStrict) {
				dominated[j] = true;
			}
		}
	}

	std::vector<std::vector<std::string>> frontier;
	for (std::size_t i = 0; i < candidates.size(); ++i) {
		if (!dominated[i]) {
			frontier.push_back(candidates[i]);
		}
	}

	return frontier;
}