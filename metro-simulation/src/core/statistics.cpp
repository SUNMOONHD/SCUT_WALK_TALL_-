#include "statistics.h"

void Statistics::reset()
{
	completedPassengers_ = 0;
	totalTravelTime_ = 0;
	timedOutPassengers_ = 0;
	congestionEvents_ = 0;
	maxQueueLength_ = 0;
	congestionCountByNode_.clear();
}

void Statistics::recordPassengerCompleted(int travelTimeSeconds)
{
	++completedPassengers_;
	totalTravelTime_ += travelTimeSeconds;
}

void Statistics::recordPassengerTimedOut()
{
	++timedOutPassengers_;
}

void Statistics::recordCongestionEvent(const std::string &nodeId)
{
	++congestionEvents_;
	++congestionCountByNode_[nodeId];
}

void Statistics::recordQueueLength(int queueLength)
{
	if (queueLength > maxQueueLength_) maxQueueLength_ = queueLength;
}

int Statistics::completedPassengers() const
{
	return completedPassengers_;
}

int Statistics::timedOutPassengers() const
{
	return timedOutPassengers_;
}

int Statistics::congestionEvents() const
{
	return congestionEvents_;
}

int Statistics::maxQueueLength() const
{
	return maxQueueLength_;
}

double Statistics::averageTravelTime() const
{
	if (completedPassengers_ == 0) {
		return 0.0;
	}

	return static_cast<double>(totalTravelTime_) / static_cast<double>(completedPassengers_);
}

const std::unordered_map<std::string, int>& Statistics::congestionCountByNode() const
{
	return congestionCountByNode_;
}
