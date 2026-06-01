#include "metro_graph.h"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

using nlohmann::json;

namespace {

bool setError(std::string *error, const std::string &message)
{
	if (error != nullptr) {
		*error = message;
	}
	return false;
}

bool isFinite(double value)
{
	return std::isfinite(value);
}

bool parseNode(const json &nodeJson, StationNode *node, std::string *error)
{
	if (node == nullptr) {
		return setError(error, "Internal error: null StationNode output.");
	}

	if (!nodeJson.is_object()) {
		return setError(error, "nodes[] item must be an object.");
	}

	if (!nodeJson.contains("id") || !nodeJson["id"].is_string()) {
		return setError(error, "nodes[] requires string field 'id'.");
	}
	if (!nodeJson.contains("type") || !nodeJson["type"].is_string()) {
		return setError(error, "nodes[] requires string field 'type'.");
	}

	node->id = nodeJson["id"].get<std::string>();
	node->name = nodeJson.value("name", node->id);
	node->type = nodeJson["type"].get<std::string>();
	node->floor = nodeJson.value("floor", 0);
	node->x = nodeJson.value("x", 0.0);
	node->y = nodeJson.value("y", 0.0);
	node->capacity = nodeJson.value("capacity", 0.0);
	node->width = nodeJson.value("width", 0.0);

	if (node->id.empty()) {
		return setError(error, "nodes[] field 'id' cannot be empty.");
	}
	if (node->type.empty()) {
		return setError(error, "nodes[] field 'type' cannot be empty.");
	}
	if (!isFinite(node->x) || !isFinite(node->y)) {
		return setError(error, "nodes[] coordinate fields must be finite numbers.");
	}
	if (!isFinite(node->capacity) || node->capacity <= 0.0) {
		return setError(error, "nodes[] field 'capacity' must be > 0.");
	}
	if (!isFinite(node->width) || node->width <= 0.0) {
		return setError(error, "nodes[] field 'width' must be > 0.");
	}

	return true;
}

bool parseEdge(const json &edgeJson, GraphEdge *edge, std::string *error)
{
	if (edge == nullptr) {
		return setError(error, "Internal error: null GraphEdge output.");
	}

	if (!edgeJson.is_object()) {
		return setError(error, "edges[] item must be an object.");
	}

	if (!edgeJson.contains("from") || !edgeJson["from"].is_string() || edgeJson["from"].get<std::string>().empty()) {
		return setError(error, "edges[] requires non-empty string field 'from'.");
	}
	if (!edgeJson.contains("to") || !edgeJson["to"].is_string() || edgeJson["to"].get<std::string>().empty()) {
		return setError(error, "edges[] requires non-empty string field 'to'.");
	}
	if (!edgeJson.contains("length") || !edgeJson["length"].is_number()) {
		return setError(error, "edges[] requires numeric field 'length'.");
	}
	if (!edgeJson.contains("width") || !edgeJson["width"].is_number()) {
		return setError(error, "edges[] requires numeric field 'width'.");
	}

	edge->from = edgeJson["from"].get<std::string>();
	edge->to = edgeJson["to"].get<std::string>();
	edge->length = edgeJson["length"].get<double>();
	edge->width = edgeJson["width"].get<double>();
	edge->bidirectional = edgeJson.value("bidirectional", true);
	edge->lineIndex = edgeJson.value("line_index", 1);

	const double capacityFallback = edge->width * 1.2;
	edge->capacity = edgeJson.value("capacity", capacityFallback);

	const double transferTimeFallback = edge->length / 1.2;
	edge->transferTime = edgeJson.value("transfer_time", transferTimeFallback);

	if (!isFinite(edge->length) || edge->length <= 0.0) {
		return setError(error, "edges[] field 'length' must be > 0.");
	}
	if (!isFinite(edge->width) || edge->width <= 0.0) {
		return setError(error, "edges[] field 'width' must be > 0.");
	}
	if (!isFinite(edge->capacity) || edge->capacity <= 0.0) {
		return setError(error, "edges[] field 'capacity' must be > 0.");
	}
	if (!isFinite(edge->transferTime) || edge->transferTime <= 0.0) {
		return setError(error, "edges[] field 'transfer_time' must be > 0.");
	}
	if (edge->lineIndex <= 0) {
		return setError(error, "edges[] field 'line_index' must be a positive integer.");
	}

	return true;
}

} // namespace

void MetroGraph::setStationName(const std::string &stationName)
{
	stationName_ = stationName;
}

const std::string &MetroGraph::stationName() const
{
	return stationName_;
}

void MetroGraph::setFloors(const std::vector<int> &floors)
{
	floors_ = floors;
}

const std::vector<int> &MetroGraph::floors() const
{
	return floors_;
}

bool MetroGraph::addNode(const StationNode &node)
{
	if (node.id.empty() || node.capacity <= 0.0 || node.width <= 0.0) {
		return false;
	}
	return nodes_.emplace(node.id, node).second;
}

bool MetroGraph::addEdge(const GraphEdge &edge)
{
	if (edge.from.empty() || edge.to.empty() || edge.length <= 0.0 || edge.width <= 0.0 || edge.capacity <= 0.0 || edge.transferTime <= 0.0) {
		return false;
	}

	if (nodes_.find(edge.from) == nodes_.end() || nodes_.find(edge.to) == nodes_.end()) {
		return false;
	}

	const std::size_t index = edges_.size();
	edges_.push_back(edge);
	adjacency_[edge.from].push_back(index);
	if (edge.bidirectional) {
		adjacency_[edge.to].push_back(index);
	}
	return true;
}

void MetroGraph::clear()
{
	stationName_.clear();
	floors_.clear();
	nodes_.clear();
	edges_.clear();
	adjacency_.clear();
}

bool MetroGraph::loadFromJsonFile(const std::string &path, std::string *error)
{
	std::ifstream input(std::filesystem::path(path).c_str(), std::ios::binary);
	if (!input.is_open()) {
		return setError(error, "Unable to open station file: " + path);
	}

	json root;
	try {
		input >> root;
	} catch (const std::exception &exception) {
		return setError(error, "Invalid station JSON: " + std::string(exception.what()));
	}

	if (!root.is_object()) {
		return setError(error, "Station root must be a JSON object.");
	}

	if (!root.contains("station_name") || !root["station_name"].is_string() || root["station_name"].get<std::string>().empty()) {
		return setError(error, "Station JSON requires non-empty string field 'station_name'.");
	}

	if (!root.contains("floors") || !root["floors"].is_array() || root["floors"].empty()) {
		return setError(error, "Station JSON requires non-empty array field 'floors'.");
	}

	std::vector<int> parsedFloors;
	parsedFloors.reserve(root["floors"].size());
	for (const auto &floorJson : root["floors"]) {
		if (!floorJson.is_number_integer()) {
			return setError(error, "floors[] must contain integers.");
		}
		parsedFloors.push_back(floorJson.get<int>());
	}

	if (!root.contains("nodes") || !root["nodes"].is_array() || root["nodes"].empty()) {
		return setError(error, "Station JSON requires non-empty array field 'nodes'.");
	}
	if (!root.contains("edges") || !root["edges"].is_array() || root["edges"].empty()) {
		return setError(error, "Station JSON requires non-empty array field 'edges'.");
	}

	MetroGraph parsedGraph;
	parsedGraph.setStationName(root["station_name"].get<std::string>());
	parsedGraph.setFloors(parsedFloors);

	for (std::size_t index = 0; index < root["nodes"].size(); ++index) {
		StationNode node;
		std::string nodeError;
		if (!parseNode(root["nodes"][index], &node, &nodeError)) {
			std::ostringstream builder;
			builder << "nodes[" << index << "]: " << nodeError;
			return setError(error, builder.str());
		}

		const bool knownFloor = std::find(parsedFloors.begin(), parsedFloors.end(), node.floor) != parsedFloors.end();
		if (!knownFloor) {
			std::ostringstream builder;
			builder << "nodes[" << index << "] has floor " << node.floor << " not declared in floors[].";
			return setError(error, builder.str());
		}

		if (!parsedGraph.addNode(node)) {
			std::ostringstream builder;
			builder << "nodes[" << index << "] has duplicate or invalid id '" << node.id << "'.";
			return setError(error, builder.str());
		}
	}

	for (std::size_t index = 0; index < root["edges"].size(); ++index) {
		GraphEdge edge;
		std::string edgeError;
		if (!parseEdge(root["edges"][index], &edge, &edgeError)) {
			std::ostringstream builder;
			builder << "edges[" << index << "]: " << edgeError;
			return setError(error, builder.str());
		}

		if (!parsedGraph.addEdge(edge)) {
			std::ostringstream builder;
			builder << "edges[" << index << "] references unknown node or has invalid values (" << edge.from << " -> " << edge.to << ").";
			return setError(error, builder.str());
		}
	}

	*this = std::move(parsedGraph);
	return true;
}

std::size_t MetroGraph::nodeCount() const
{
	return nodes_.size();
}

std::size_t MetroGraph::edgeCount() const
{
	return edges_.size();
}

const std::unordered_map<std::string, StationNode> &MetroGraph::nodes() const
{
	return nodes_;
}

const std::vector<GraphEdge> &MetroGraph::edges() const
{
	return edges_;
}

const std::unordered_map<std::string, std::vector<std::size_t>> &MetroGraph::adjacency() const
{
	return adjacency_;
}
