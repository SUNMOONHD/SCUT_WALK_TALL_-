#pragma once

#include <string>
#include <unordered_map>

class Statistics {
public:
	void reset();
	void recordPassengerCompleted(int travelTimeSeconds);
	void recordPassengerTimedOut();
	void recordCongestionEvent(const std::string &nodeId);
	void recordQueueLength(int queueLength);

	int completedPassengers() const;
	int timedOutPassengers() const;
	int congestionEvents() const;
	int maxQueueLength() const;
	double averageTravelTime() const;
	const std::unordered_map<std::string, int>& congestionCountByNode() const;

private:
	int completedPassengers_ = 0;
	int timedOutPassengers_ = 0;
	int congestionEvents_ = 0;
	int maxQueueLength_ = 0;
	long long totalTravelTime_ = 0;
	std::unordered_map<std::string, int> congestionCountByNode_;
};
