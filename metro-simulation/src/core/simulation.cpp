#include "simulation.h"

#include "json.hpp"
#include "path_planner.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

using nlohmann::json;

namespace {

bool setError(std::string *error, const std::string &message)
{
	if (error != nullptr) {
		*error = message;
	}
	return false;
}

bool isValidHourRange(int startHour, int endHour)
{
	return startHour >= 0 && startHour <= 23 && endHour >= 0 && endHour <= 23 && startHour < endHour;
}

bool isPeakHour(int hour, const std::vector<std::pair<int, int>> &peakHours)
{
	for (const auto &range : peakHours) {
		if (hour >= range.first && hour < range.second) {
			return true;
		}
	}
	return false;
}

double clampDouble(double value, double minValue, double maxValue)
{
	if (value < minValue) return minValue;
	if (value > maxValue) return maxValue;
	return value;
}

double nodeTypeSpeedLimit(const std::string &type)
{
	if (type == "escalator") return 0.65;
	if (type == "stairs") return 0.5;
	return 1.2;
}

int samplePassengerCount(std::mt19937 &gen, const SimulationConfig &config, int hour, int timeStepSeconds, int nodeCount)
{
	const bool peak = isPeakHour(hour, config.peakHours);
	const double nodeScale = std::max(1.0, static_cast<double>(nodeCount) / std::max(1.0, config.baseNodeCount));
	const double baseLambda = peak ? config.peakLambda : config.offpeakLambda;
	const double multiplier = peak ? config.peakMultiplier : 1.0;
	const double lambdaPerMinute = baseLambda * nodeScale * multiplier;
	const double baseMean = (lambdaPerMinute / 60.0) * static_cast<double>(timeStepSeconds);
	double expected = baseMean;
	std::poisson_distribution<int> basePoisson(std::max(0.0, baseMean));
	int arrivals = basePoisson(gen);

	if (config.burstMean > 0.0) {
		const double burstChance = peak ? 0.28 : 0.08;
		std::bernoulli_distribution burstTriggered(burstChance);
		if (burstTriggered(gen)) {
			const double stddev = std::max(1.0, config.burstStdDev);
			std::normal_distribution<double> burstDist(config.burstMean, stddev);
			const int burst = static_cast<int>(std::round(std::max(0.0, burstDist(gen))));
			arrivals += burst;
		}
	}

	if (arrivals == 0 && expected > 0.6) {
		std::bernoulli_distribution oneMore(expected - std::floor(expected));
		if (oneMore(gen)) {
			arrivals = 1;
		}
	}

	return std::max(0, arrivals);
}

std::string pickRandomNode(std::mt19937 &gen, const std::vector<std::string> &nodes)
{
	if (nodes.empty()) {
		return {};
	}
	std::uniform_int_distribution<std::size_t> dist(0, nodes.size() - 1);
	return nodes[dist(gen)];
}

void assignNodeProcessing(Passenger &p, const StationNode &node, const ProcessingConfig &procConfig)
{
	p.waitedSeconds = 0.0;
	if (node.type == "security") {
		p.state = PassengerState::Security;
		p.nodeWaitRemaining = static_cast<double>(procConfig.securityTime);
	} else if (node.type == "ticket") {
		p.state = PassengerState::Ticket;
		double familiarityFactor = 1.0 - p.familiarity * 0.6;
		p.nodeWaitRemaining = static_cast<double>(procConfig.ticketTimeBase) * familiarityFactor;
	} else if (node.type == "gate") {
		p.state = PassengerState::Wait;
		p.nodeWaitRemaining = static_cast<double>(procConfig.gateTime);
	} else if (node.type == "platform" || node.type == "waiting") {
		p.state = PassengerState::Wait;
		p.nodeWaitRemaining = static_cast<double>(procConfig.trainHeadway);
	} else if (node.type == "entrance") {
		p.state = PassengerState::Enter;
		p.nodeWaitRemaining = static_cast<double>(procConfig.entryDwellTime);
	} else if (node.type == "exit") {
		p.state = PassengerState::Exit;
		p.nodeWaitRemaining = static_cast<double>(procConfig.exitDwellTime);
	} else {
		p.state = PassengerState::Wait;
		p.nodeWaitRemaining = 0.0;
	}
}

} // namespace

Simulation::Simulation() = default;

void Simulation::reset()
{
	currentTime_ = 0;
	passengers_.clear();
	events_.clear();
	statistics_.reset();
	nextPassengerId_ = 1;
	nodeOccupancy_.clear();
	edgeOccupancy_.clear();
	congestedNodes_.clear();
	lastTrainTime_.clear();
	lastHour_ = -1;
	finishedPurgedCount_ = 0;
}

void Simulation::step()
{
	nodeOccupancy_.clear();
	edgeOccupancy_.clear();
	for (const auto &p : passengers_) {
		if (p.state == PassengerState::Finished) continue;
		if (p.onEdge) {
			if (p.edgeIndex >= 0) edgeOccupancy_[std::to_string(p.edgeIndex)] += 1;
		} else {
			nodeOccupancy_[p.currentNode] += 1;
		}
	}

	static thread_local std::mt19937 gen(static_cast<unsigned int>(std::time(nullptr)));
	const int hour = (currentTime_ / 3600) % 24;
	const int nodeCount = static_cast<int>(graph_.nodes().size());
	const int arrivals = samplePassengerCount(gen, config_, hour, timeStep_, nodeCount);

	if (arrivals >= 8) {
		events_.emplace_back(EventType::PassengerSurge, currentTime_, "", -1,
			"客流爆发 — 本步到达 " + std::to_string(arrivals) + " 人");
	}

	if (hour != lastHour_) {
		bool wasPeak = isPeakHour(lastHour_, config_.peakHours);
		bool isPeak = isPeakHour(hour, config_.peakHours);
		if (!wasPeak && isPeak) {
			events_.emplace_back(EventType::PeakHourStarted, currentTime_, "", -1,
				"早高峰开始 " + std::to_string(hour) + ":00 — 客流量激增");
		} else if (wasPeak && !isPeak) {
			events_.emplace_back(EventType::PeakHourEnded, currentTime_, "", -1,
				std::to_string(hour) + ":00 高峰结束 — 客流量回落");
		}
		lastHour_ = hour;
	}

	std::vector<std::string> entries, exits, platforms;
	for (const auto &kv : graph_.nodes()) {
		const auto &n = kv.second;
		if (n.type == "entrance") entries.push_back(n.id);
		if (n.type == "exit") exits.push_back(n.id);
		if (n.type == "platform") platforms.push_back(n.id);
	}

	std::uniform_real_distribution<double> uniform01(0.0, 1.0);
	std::normal_distribution<double> speedFactor(1.0, 0.15);
	std::uniform_real_distribution<double> patienceFactor(0.55, 1.15);

	auto nodeName = [&](const std::string &nodeId) -> std::string {
		auto it = graph_.nodes().find(nodeId);
		if (it != graph_.nodes().end() && !it->second.name.empty()) return it->second.name;
		return nodeId;
	};

	auto generatePassenger = [&](const std::string &start, const std::string &dest, const std::string &tag) {
		if (start.empty() || dest.empty() || start == dest) return;
		Passenger p(nextPassengerId_++, start, dest);
		p.arrivalTime = currentTime_;
		p.speed = clampDouble(config_.baseSpeed * speedFactor(gen), 0.5, 2.5);
		p.patience = clampDouble(static_cast<double>(config_.maxPatience) * patienceFactor(gen), 30.0, static_cast<double>(config_.maxPatience) * 1.5);
		p.familiarity = uniform01(gen);

		p.path = pathPlanner_.findPath(
			graph_, p.startNode, p.endNode,
			config_.pathObjective, config_.pathWeights,
			edgeOccupancy_, nodeOccupancy_);
		p.pathIndex = 0;
		p.currentNode = p.startNode;
		if (!p.path.empty() && p.path.size() > 1) p.targetNode = p.path[1];

		auto nodeIt = graph_.nodes().find(p.currentNode);
		if (nodeIt != graph_.nodes().end()) {
			assignNodeProcessing(p, nodeIt->second, config_.processing);
		}

		nodeOccupancy_[p.currentNode] += 1;
		passengers_.push_back(p);

		std::string eventMsg = tag + " [速度=" + std::to_string(p.speed).substr(0, 4)
			+ " 耐心=" + std::to_string(static_cast<int>(p.patience))
			+ "s 熟悉=" + std::to_string(static_cast<int>(p.familiarity * 100)) + "%]";
		events_.emplace_back(EventType::PassengerArrived, currentTime_, nodeName(p.currentNode), p.id, eventMsg);
	};

	bool isMorningPeak = (hour >= 7 && hour < 9);
	bool isMidday      = (hour >= 11 && hour < 13);
	bool isEveningPeak = (hour >= 17 && hour < 19);

	double rEntryPlat = 0.25, rEntryExit = 0.25, rPlatExit = 0.25, rTransfer = 0.25;
	if (isMorningPeak) {
		rEntryPlat = 0.50; rEntryExit = 0.10; rPlatExit = 0.15; rTransfer = 0.25;
	} else if (isMidday) {
		rEntryPlat = 0.25; rEntryExit = 0.25; rPlatExit = 0.25; rTransfer = 0.25;
	} else if (isEveningPeak) {
		rEntryPlat = 0.15; rEntryExit = 0.10; rPlatExit = 0.45; rTransfer = 0.30;
	}

	int entryArrivals = static_cast<int>(arrivals * (rEntryPlat + rEntryExit));
	int entryToPlat  = static_cast<int>(entryArrivals * (rEntryPlat / (rEntryPlat + rEntryExit)));
	int entryToExit  = entryArrivals - entryToPlat;

	if (!entries.empty() && !platforms.empty()) {
		std::uniform_int_distribution<std::size_t> entryDist(0, entries.size() - 1);
		std::uniform_int_distribution<std::size_t> platDist(0, platforms.size() - 1);
		for (int i = 0; i < entryToPlat; ++i) {
			generatePassenger(entries[entryDist(gen)], platforms[platDist(gen)], "进站→乘车");
		}
	}
	if (!entries.empty() && !exits.empty()) {
		std::uniform_int_distribution<std::size_t> entryDist(0, entries.size() - 1);
		std::uniform_int_distribution<std::size_t> exitDist(0, exits.size() - 1);
		for (int i = 0; i < entryToExit; ++i) {
			generatePassenger(entries[entryDist(gen)], exits[exitDist(gen)], "进站→出站");
		}
	}

	int headway = config_.processing.trainHeadway;
	if (isMorningPeak || isEveningPeak) headway = std::max(40, headway / 2);
	else if (isMidday) headway = std::max(60, headway * 2 / 3);

	for (const auto &platId : platforms) {
		int &lastTime = lastTrainTime_[platId];
		if (currentTime_ - lastTime >= headway || lastTime == 0) {
			lastTime = currentTime_;
			events_.emplace_back(EventType::TrainArrived, currentTime_, platId, -1,
				"列车到达 " + nodeName(platId) + " — 下车乘客涌入");

			int trainPassengers = static_cast<int>(arrivals * (rPlatExit + rTransfer));
			int platToExit = static_cast<int>(trainPassengers * (rPlatExit / (rPlatExit + rTransfer)));
			int platToPlat = trainPassengers - platToExit;

			if (!exits.empty()) {
				std::uniform_int_distribution<std::size_t> exitDist(0, exits.size() - 1);
				for (int i = 0; i < platToExit; ++i) {
					generatePassenger(platId, exits[exitDist(gen)], "下车→出站");
				}
			}

			if (platforms.size() >= 2) {
				std::uniform_int_distribution<std::size_t> platDist(0, platforms.size() - 1);
				for (int i = 0; i < platToPlat; ++i) {
					std::string dst = platId;
					while (dst == platId) dst = platforms[platDist(gen)];
					generatePassenger(platId, dst, "换乘");
				}
			}
		}
	}

	for (auto &p : passengers_) {
		if (p.state == PassengerState::Finished) continue;

		if (p.onEdge) {
			p.edgeTravelRemaining -= static_cast<double>(timeStep_);
			if (p.edgeTravelRemaining <= 0.0) {
				p.onEdge = false;
				if (p.edgeIndex >= 0) {
					auto key = std::to_string(p.edgeIndex);
					if (edgeOccupancy_.find(key) != edgeOccupancy_.end()) {
						--edgeOccupancy_[key];
					}
				}
				p.currentNode = p.edgeTo;
				nodeOccupancy_[p.currentNode] += 1;

				auto nodeIt = graph_.nodes().find(p.currentNode);
				if (nodeIt != graph_.nodes().end()) {
					assignNodeProcessing(p, nodeIt->second, config_.processing);
				}

				events_.emplace_back(EventType::PassengerArrived, currentTime_, nodeName(p.currentNode), p.id, "到达节点");

				if (p.currentNode == p.endNode && p.nodeWaitRemaining <= 0.0) {
					auto endNodeIt = graph_.nodes().find(p.currentNode);
					bool isPlatform = (endNodeIt != graph_.nodes().end() && endNodeIt->second.type == "platform");
					p.state = isPlatform ? PassengerState::Board : PassengerState::Exit;
					p.nodeWaitRemaining = 0.0;
				}
			}
			continue;
		}

		if (p.nodeWaitRemaining > 0.0) {
			p.nodeWaitRemaining -= static_cast<double>(timeStep_);
			p.waitedSeconds += static_cast<double>(timeStep_);
			if (p.nodeWaitRemaining > 0.0) {
				continue;
			}
			p.nodeWaitRemaining = 0.0;
			p.waitedSeconds = 0.0;
		}

		if (p.currentNode == p.endNode) {
			auto endNodeIt = graph_.nodes().find(p.currentNode);
			bool isPlatform = (endNodeIt != graph_.nodes().end() && endNodeIt->second.type == "platform");
			p.state = isPlatform ? PassengerState::Board : PassengerState::Exit;
			p.nodeWaitRemaining = 0.0;
			continue;
		}

		if (p.path.empty() || p.pathIndex + 1 >= p.path.size()) {
			p.path = pathPlanner_.findPath(
				graph_, p.currentNode, p.endNode,
				config_.pathObjective, config_.pathWeights,
				edgeOccupancy_, nodeOccupancy_);
			p.pathIndex = 0;
		}

		if (p.pathIndex + 1 < p.path.size()) {
			const std::string &next = p.path[p.pathIndex + 1];

			auto nextNodeIt = graph_.nodes().find(next);
			if (nextNodeIt != graph_.nodes().end()) {
				double nodeDensity = 0.0;
				auto occIt = nodeOccupancy_.find(next);
				if (occIt != nodeOccupancy_.end() && nextNodeIt->second.capacity > 0.0) {
					nodeDensity = static_cast<double>(occIt->second) / nextNodeIt->second.capacity;
				}

				if (nodeDensity > config_.congestionThreshold) {
					auto altPath = pathPlanner_.findPath(
						graph_, p.currentNode, p.endNode,
						PathObjective::MinCongestion, {},
						edgeOccupancy_, nodeOccupancy_);
					if (!altPath.empty() && altPath.size() > 1) {
						bool avoidsCongestion = true;
						for (std::size_t pi = 1; pi < altPath.size(); ++pi) {
							if (altPath[pi] == next) {
								avoidsCongestion = false;
								break;
							}
						}
						if (avoidsCongestion) {
							p.path = altPath;
							p.pathIndex = 0;
							std::string reason = "绕行重规划";
							if (p.patience < 120.0) {
								reason = "耐心度低(" + std::to_string(static_cast<int>(p.patience)) + "s)，选择绕行";
							} else if (p.familiarity < 0.3) {
								reason = "熟悉度低(" + std::to_string(static_cast<int>(p.familiarity * 100)) + "%)，尝试新路径";
							}
							events_.emplace_back(EventType::PassengerArrived, currentTime_, nodeName(p.currentNode), p.id, reason);
						}
					}
				}
			}

			if (p.pathIndex + 1 >= p.path.size()) {
				continue;
			}

			const std::string &actualNext = p.path[p.pathIndex + 1];
			int edgeIdx = -1;
			for (std::size_t ei = 0; ei < graph_.edges().size(); ++ei) {
				const auto &e = graph_.edges()[ei];
				if ((e.from == p.currentNode && e.to == actualNext) || (e.bidirectional && e.from == actualNext && e.to == p.currentNode)) {
					edgeIdx = static_cast<int>(ei);
					break;
				}
			}

			if (edgeIdx < 0) {
				p.state = PassengerState::Finished;
				events_.emplace_back(EventType::TimeoutReached, currentTime_, nodeName(p.currentNode), p.id, "无可用边");
				continue;
			}

			const auto &edge = graph_.edges()[edgeIdx];
			int occ = edgeOccupancy_[std::to_string(edgeIdx)];
			int cap = static_cast<int>(std::max(1.0, std::floor(edge.capacity)));
			if (occ < cap) {
				if (nodeOccupancy_.find(p.currentNode) != nodeOccupancy_.end() && nodeOccupancy_[p.currentNode] > 0) --nodeOccupancy_[p.currentNode];
				p.onEdge = true;
				p.edgeFrom = p.currentNode;
				p.edgeTo = actualNext;
				p.edgeIndex = edgeIdx;
				p.waitedSeconds = 0.0;

				double occupancyRatio = static_cast<double>(occ) / std::max(1.0, edge.capacity);
				double speedFactor = 1.0;
				if (occupancyRatio >= config_.congestionThreshold) {
					speedFactor = config_.congestionK;
				} else if (occupancyRatio > 0.3) {
					speedFactor = 1.0 - (occupancyRatio * 0.2);
				}
				speedFactor = std::max(0.2, speedFactor);

				double effectiveSpeed = p.speed;
				auto fromNodeIt2 = graph_.nodes().find(p.edgeFrom);
				auto toNodeIt2 = graph_.nodes().find(p.edgeTo);
				if (fromNodeIt2 != graph_.nodes().end())
					effectiveSpeed = std::min(effectiveSpeed, nodeTypeSpeedLimit(fromNodeIt2->second.type));
				if (toNodeIt2 != graph_.nodes().end())
					effectiveSpeed = std::min(effectiveSpeed, nodeTypeSpeedLimit(toNodeIt2->second.type));

				p.edgeTravelTotal = edge.length / (effectiveSpeed * speedFactor);
				p.edgeTravelRemaining = p.edgeTravelTotal;
				p.progress = 0.0;
				++edgeOccupancy_[std::to_string(edgeIdx)];
				++p.pathIndex;
				events_.emplace_back(EventType::PassengerArrived, currentTime_, nodeName(p.currentNode), p.id, "进入通道");
			} else {
				p.waitedSeconds += static_cast<double>(timeStep_);
				if (p.waitedSeconds >= p.patience) {
					p.state = PassengerState::Finished;
					p.exitTime = currentTime_;
					statistics_.recordPassengerTimedOut();
					std::string msg = "耐心耗尽(等待" + std::to_string(static_cast<int>(p.waitedSeconds))
						+ "s/耐心" + std::to_string(static_cast<int>(p.patience)) + "s)，离开";
					events_.emplace_back(EventType::TimeoutReached, currentTime_, nodeName(p.currentNode), p.id, msg);
					if (nodeOccupancy_.find(p.currentNode) != nodeOccupancy_.end() && nodeOccupancy_[p.currentNode] > 0) --nodeOccupancy_[p.currentNode];
				}
			}
		} else {
			p.state = PassengerState::Finished;
		}
	}

	for (const auto &kv : nodeOccupancy_) {
		const auto nodeIt = graph_.nodes().find(kv.first);
		if (nodeIt == graph_.nodes().end()) continue;
		const auto &node = nodeIt->second;
		double density = static_cast<double>(kv.second) / node.capacity;
		statistics_.recordQueueLength(kv.second);
		if (density > config_.congestionThreshold) {
			if (congestedNodes_.find(kv.first) == congestedNodes_.end()) {
				congestedNodes_.insert(kv.first);
				statistics_.recordCongestionEvent(kv.first);
				events_.emplace_back(EventType::CongestionTriggered, currentTime_, nodeName(kv.first), -1, "拥堵触发");
			}
		} else {
			congestedNodes_.erase(kv.first);
		}
	}

	{
		std::unordered_map<std::string, std::vector<Passenger *>> platformBoarders;
		for (auto &p : passengers_) {
			if (p.state == PassengerState::Board) {
				platformBoarders[p.currentNode].push_back(&p);
			}
		}

		const int capacity = config_.processing.trainCapacity;
		for (auto &[platId, boarders] : platformBoarders) {
			if (static_cast<int>(boarders.size()) <= capacity) continue;

			std::sort(boarders.begin(), boarders.end(),
				[](const Passenger *a, const Passenger *b) {
					return a->arrivalTime < b->arrivalTime;
				});

			for (int i = capacity; i < static_cast<int>(boarders.size()); ++i) {
				boarders[i]->state = PassengerState::Wait;
				boarders[i]->nodeWaitRemaining = static_cast<double>(config_.processing.trainHeadway);
				boarders[i]->waitedSeconds = 0.0;
				events_.emplace_back(EventType::PassengerArrived, currentTime_,
					nodeName(boarders[i]->currentNode), boarders[i]->id,
					"列车满员(" + std::to_string(capacity) + "人)，等待下一班("
						+ std::to_string(config_.processing.trainHeadway) + "s)");
			}
		}
	}

	for (auto &p : passengers_) {
		if (p.state == PassengerState::Exit || p.state == PassengerState::Board) {
			bool wasBoard = (p.state == PassengerState::Board);
			p.state = PassengerState::Finished;
			p.exitTime = currentTime_;
			statistics_.recordPassengerCompleted(p.exitTime - p.arrivalTime);
			const char *msg = wasBoard ? "乘车完成" : "出站完成";
			events_.emplace_back(EventType::PassengerExited, currentTime_, nodeName(p.currentNode), p.id, msg);
			if (nodeOccupancy_.find(p.currentNode) != nodeOccupancy_.end() && nodeOccupancy_[p.currentNode] > 0) --nodeOccupancy_[p.currentNode];
		}
	}

	auto oldSize = passengers_.size();
	passengers_.erase(
		std::remove_if(passengers_.begin(), passengers_.end(),
			[](const Passenger &p) { return p.state == PassengerState::Finished; }),
		passengers_.end());
	finishedPurgedCount_ += static_cast<int>(oldSize - passengers_.size());

	currentTime_ += timeStep_;

	const std::size_t kMaxEvents = 1000;
	if (events_.size() > kMaxEvents) {
		events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() - kMaxEvents));
	}
}

void Simulation::setTimeStep(int timeStep)
{
	timeStep_ = timeStep > 0 ? timeStep : 1;
	config_.timeStep = timeStep_;
}

int Simulation::timeStep() const
{
	return timeStep_;
}

int Simulation::currentTime() const
{
	return currentTime_;
}

void Simulation::setConfig(const SimulationConfig &config)
{
	config_ = config;
	timeStep_ = config_.timeStep > 0 ? config_.timeStep : 1;
	pathPlanner_.setUseAStar(config_.useAStar);
	pathPlanner_.setCacheEnabled(config_.usePathCache);
}

const SimulationConfig &Simulation::config() const
{
	return config_;
}

bool Simulation::loadConfigFromJsonFile(const std::string &path, std::string *error)
{
	std::ifstream input(std::filesystem::path(path).c_str(), std::ios::binary);
	if (!input.is_open()) {
		return setError(error, "Unable to open params file: " + path);
	}

	json root;
	try {
		input >> root;
	} catch (const std::exception &exception) {
		return setError(error, "Invalid params JSON: " + std::string(exception.what()));
	}

	if (!root.is_object()) {
		return setError(error, "Params root must be a JSON object.");
	}

	SimulationConfig parsed;
	parsed.timeStep = root.value("time_step", parsed.timeStep);
	parsed.peakLambda = root.value("peak_lambda", parsed.peakLambda);
	parsed.offpeakLambda = root.value("offpeak_lambda", parsed.offpeakLambda);
	parsed.peakMultiplier = root.value("peak_multiplier", parsed.peakMultiplier);
	parsed.baseNodeCount = root.value("base_node_count", parsed.baseNodeCount);
	parsed.maxPatience = root.value("max_patience", parsed.maxPatience);
	parsed.baseSpeed = root.value("base_speed", parsed.baseSpeed);
	parsed.congestionThreshold = root.value("congestion_threshold", parsed.congestionThreshold);
	parsed.congestionK = root.value("congestion_k", parsed.congestionK);
	parsed.burstMean = root.value("burst_mean", parsed.burstMean);
	parsed.burstStdDev = root.value("burst_stddev", parsed.burstStdDev);

	parsed.processing.securityTime = root.value("security_time", parsed.processing.securityTime);
	parsed.processing.ticketTimeBase = root.value("ticket_time_base", parsed.processing.ticketTimeBase);
	parsed.processing.gateTime = root.value("gate_time", parsed.processing.gateTime);
	parsed.processing.boardingTime = root.value("boarding_time", parsed.processing.boardingTime);
	parsed.processing.trainHeadway = root.value("train_headway", parsed.processing.trainHeadway);
	parsed.processing.trainCapacity = root.value("train_capacity", parsed.processing.trainCapacity);
	parsed.processing.entryDwellTime = root.value("entry_dwell_time", parsed.processing.entryDwellTime);
	parsed.processing.exitDwellTime = root.value("exit_dwell_time", parsed.processing.exitDwellTime);

	if (root.contains("peak_hours")) {
		if (!root["peak_hours"].is_array()) {
			return setError(error, "params field 'peak_hours' must be an array.");
		}

		for (std::size_t index = 0; index < root["peak_hours"].size(); ++index) {
			const auto &range = root["peak_hours"][index];
			if (!range.is_array() || range.size() != 2 || !range[0].is_number_integer() || !range[1].is_number_integer()) {
				std::ostringstream builder;
				builder << "peak_hours[" << index << "] must be [start_hour, end_hour].";
				return setError(error, builder.str());
			}

			const int startHour = range[0].get<int>();
			const int endHour = range[1].get<int>();
			if (!isValidHourRange(startHour, endHour)) {
				std::ostringstream builder;
				builder << "peak_hours[" << index << "] must satisfy 0<=start<end<=23.";
				return setError(error, builder.str());
			}

			parsed.peakHours.emplace_back(startHour, endHour);
		}
	}

	if (parsed.timeStep <= 0 || parsed.timeStep > 60) {
		return setError(error, "params field 'time_step' must be in range [1, 60].");
	}
	if (!std::isfinite(parsed.peakLambda) || parsed.peakLambda < 0.0) {
		return setError(error, "params field 'peak_lambda' must be >= 0.");
	}
	if (!std::isfinite(parsed.offpeakLambda) || parsed.offpeakLambda < 0.0) {
		return setError(error, "params field 'offpeak_lambda' must be >= 0.");
	}
	if (!std::isfinite(parsed.peakMultiplier) || parsed.peakMultiplier < 0.1) {
		return setError(error, "params field 'peak_multiplier' must be >= 0.1.");
	}
	if (!std::isfinite(parsed.baseNodeCount) || parsed.baseNodeCount < 1.0) {
		return setError(error, "params field 'base_node_count' must be >= 1.");
	}
	if (parsed.maxPatience <= 0) {
		return setError(error, "params field 'max_patience' must be > 0.");
	}
	if (!std::isfinite(parsed.baseSpeed) || parsed.baseSpeed <= 0.0 || parsed.baseSpeed > 5.0) {
		return setError(error, "params field 'base_speed' must be in range (0, 5].");
	}
	if (!std::isfinite(parsed.congestionThreshold) || parsed.congestionThreshold <= 0.0 || parsed.congestionThreshold >= 1.0) {
		return setError(error, "params field 'congestion_threshold' must be in range (0, 1).");
	}
	if (!std::isfinite(parsed.congestionK) || parsed.congestionK < 0.0 || parsed.congestionK > 1.0) {
		return setError(error, "params field 'congestion_k' must be in range [0, 1].");
	}
	if (!std::isfinite(parsed.burstMean) || parsed.burstMean < 0.0) {
		return setError(error, "params field 'burst_mean' must be >= 0.");
	}
	if (!std::isfinite(parsed.burstStdDev) || parsed.burstStdDev < 0.0) {
		return setError(error, "params field 'burst_stddev' must be >= 0.");
	}

	setConfig(parsed);
	return true;
}

bool Simulation::loadScenario(const std::string &stationPath, const std::string &paramsPath, std::string *error)
{
	MetroGraph loadedGraph;
	std::string graphError;
	if (!loadedGraph.loadFromJsonFile(stationPath, &graphError)) {
		return setError(error, "Station load failed: " + graphError);
	}

	Simulation loadedSimulation;
	std::string configError;
	if (!loadedSimulation.loadConfigFromJsonFile(paramsPath, &configError)) {
		return setError(error, "Params load failed: " + configError);
	}

	setConfig(loadedSimulation.config());
	setGraph(loadedGraph);
	return true;
}

void Simulation::setGraph(const MetroGraph &graph)
{
	graph_ = graph;
}

const MetroGraph &Simulation::graph() const
{
	return graph_;
}

void Simulation::addPassenger(const Passenger &passenger)
{
	passengers_.push_back(passenger);
}

const std::vector<Passenger> &Simulation::passengers() const
{
	return passengers_;
}

const std::vector<Event> &Simulation::events() const
{
	return events_;
}

const Statistics &Simulation::statistics() const
{
	return statistics_;
}

const std::unordered_map<std::string, int> &Simulation::nodeOccupancy() const
{
	return nodeOccupancy_;
}

const std::unordered_map<std::string, int> &Simulation::edgeOccupancy() const
{
	return edgeOccupancy_;
}