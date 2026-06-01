#pragma once

#include "metro_graph.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class PathObjective {
	MinTime,
	MinDistance,
	MinCongestion,
	MinZoneSwitches,
	WeightedSum
};

struct PathWeights {
	double wTime = 0.4;
	double wDistance = 0.2;
	double wCongestion = 0.3;
	double wZoneSwitch = 0.1;
};

struct PathMetrics {
	double totalTime = 0.0;
	double totalDistance = 0.0;
	double avgCongestion = 0.0;
	int zoneSwitches = 0;
};

class PathPlanner {
public:
	std::vector<std::string> findPath(
		const MetroGraph &graph,
		const std::string &from,
		const std::string &to,
		PathObjective objective = PathObjective::WeightedSum,
		const PathWeights &weights = {},
		const std::unordered_map<std::string, int> &edgeOccupancy = {},
		const std::unordered_map<std::string, int> &nodeOccupancy = {}
	) const;

	PathMetrics computePathMetrics(
		const MetroGraph &graph,
		const std::vector<std::string> &path,
		const std::unordered_map<std::string, int> &edgeOccupancy = {},
		const std::unordered_map<std::string, int> &nodeOccupancy = {}
	) const;

	std::vector<std::vector<std::string>> findParetoFrontier(
		const MetroGraph &graph,
		const std::string &from,
		const std::string &to,
		const std::unordered_map<std::string, int> &edgeOccupancy = {},
		const std::unordered_map<std::string, int> &nodeOccupancy = {}
	) const;

	void clearCache() { pathCache_.clear(); }
	size_t cacheSize() const { return pathCache_.size(); }
	void setCacheEnabled(bool enabled) { cacheEnabled_ = enabled; }
	void setUseAStar(bool useAStar) { useAStar_ = useAStar; }
	bool getCacheEnabled() const { return cacheEnabled_; }
	bool getUseAStar() const { return useAStar_; }

private:
	mutable bool cacheEnabled_ = true;
	mutable bool useAStar_ = true;
	mutable std::unordered_map<std::string, std::vector<std::string>> pathCache_;
};
