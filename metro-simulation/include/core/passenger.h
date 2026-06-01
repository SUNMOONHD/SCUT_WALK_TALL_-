#pragma once

#include <string>
#include <vector>

enum class PassengerState {
	Enter,
	Security,
	Ticket,
	Wait,
	Board,
	Exit,
	Finished
};

class Passenger {
public:
	Passenger() = default;
	Passenger(int passengerId, std::string startNode, std::string endNode);

	int id = -1;
	std::string startNode;
	std::string endNode;
	double speed = 1.2;
	double patience = 1.0;
	double familiarity = 0.5;
	PassengerState state = PassengerState::Enter;
	std::string currentNode;
	std::string targetNode;
	std::vector<std::string> path;
	std::size_t pathIndex = 0;
	double progress = 0.0;
	double nodeWaitRemaining = 0.0;
	double edgeTravelRemaining = 0.0;
	double edgeTravelTotal = 0.0;
	double waitedSeconds = 0.0;
	bool onEdge = false;
	std::string edgeFrom;
	std::string edgeTo;
	int edgeIndex = -1;
	int arrivalTime = 0;
	int exitTime = -1;
};
