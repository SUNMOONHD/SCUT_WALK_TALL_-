#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct StationNode {
	std::string id;
	std::string name;
	std::string type;
	int floor = 0;
	double x = 0.0;
	double y = 0.0;
	double capacity = 0.0;
	double width = 0.0;
};

struct GraphEdge {
	std::string from;
	std::string to;
	double length = 0.0;
	double width = 0.0;
	double capacity = 0.0;
	double transferTime = 0.0;
	int lineIndex = 1;
	bool bidirectional = true;
};

class MetroGraph {
public:
	void setStationName(const std::string &stationName);
	const std::string &stationName() const;

	void setFloors(const std::vector<int> &floors);
	const std::vector<int> &floors() const;

	bool addNode(const StationNode &node);
	bool addEdge(const GraphEdge &edge);
	void clear();

	bool loadFromJsonFile(const std::string &path, std::string *error);

	std::size_t nodeCount() const;
	std::size_t edgeCount() const;

	const std::unordered_map<std::string, StationNode> &nodes() const;
	const std::vector<GraphEdge> &edges() const;
	const std::unordered_map<std::string, std::vector<std::size_t>> &adjacency() const;

private:
	std::string stationName_;
	std::vector<int> floors_;
	std::unordered_map<std::string, StationNode> nodes_;
	std::vector<GraphEdge> edges_;
	std::unordered_map<std::string, std::vector<std::size_t>> adjacency_;
};
