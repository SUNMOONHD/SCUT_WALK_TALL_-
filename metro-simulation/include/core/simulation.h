#pragma once

#include "event.h"
#include "metro_graph.h"
#include "passenger.h"
#include "path_planner.h"
#include "statistics.h"

#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ProcessingConfig {
	int securityTime = 4;
	int ticketTimeBase = 8;
	int gateTime = 1;
	int boardingTime = 3;
	int trainHeadway = 120;
	int trainCapacity = 200;
	int entryDwellTime = 2;
	int exitDwellTime = 2;
};

struct SimulationConfig {
	int timeStep = 1;
	double peakLambda = 30.0;
	double offpeakLambda = 10.0;
	double peakMultiplier = 3.0;
	double baseNodeCount = 20.0;
	std::vector<std::pair<int, int>> peakHours;
	int maxPatience = 300;
	double baseSpeed = 1.2;
	double congestionThreshold = 0.7;
	double congestionK = 0.6;
	double burstMean = 0.0;
	double burstStdDev = 0.0;
	ProcessingConfig processing;
	PathObjective pathObjective = PathObjective::WeightedSum;
	PathWeights pathWeights;
	bool useAStar = true;
	bool usePathCache = true;
};

class Simulation {
public:
	Simulation();

	void reset();
	void step();

	void setTimeStep(int timeStep);
	int timeStep() const;
	int currentTime() const;

	void setConfig(const SimulationConfig &config);
	const SimulationConfig &config() const;
	bool loadConfigFromJsonFile(const std::string &path, std::string *error);

	bool loadScenario(const std::string &stationPath, const std::string &paramsPath, std::string *error);

	void setGraph(const MetroGraph &graph);
	const MetroGraph &graph() const;

	void addPassenger(const Passenger &passenger);
	const std::vector<Passenger> &passengers() const;
	const std::vector<Event> &events() const;
	const Statistics &statistics() const;
	const std::unordered_map<std::string, int> &nodeOccupancy() const;
	const std::unordered_map<std::string, int> &edgeOccupancy() const;

private:
	int timeStep_ = 1;
	int currentTime_ = 0;
	SimulationConfig config_;
	MetroGraph graph_;
	PathPlanner pathPlanner_;
	std::vector<Passenger> passengers_;
	std::vector<Event> events_;
	Statistics statistics_;
	int nextPassengerId_ = 1;
	std::unordered_map<std::string, int> nodeOccupancy_;
	std::unordered_map<std::string, int> edgeOccupancy_;
	std::unordered_set<std::string> congestedNodes_;
	std::unordered_map<std::string, int> lastTrainTime_;
	int lastHour_ = -1;
	int finishedPurgedCount_ = 0;
};
