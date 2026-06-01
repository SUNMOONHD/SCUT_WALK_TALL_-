#pragma once

#include <string>

enum class EventType {
	PassengerArrived,
	PassengerExited,
	CongestionTriggered,
	TimeoutReached,
	PeakHourStarted,
	PeakHourEnded,
	TrainArrived,
	PassengerSurge
};

struct Event {
	Event() = default;
	Event(EventType eventType, int eventTime, std::string nodeId = {}, int passengerId = -1, std::string eventMessage = {});

	EventType type = EventType::PassengerArrived;
	int time = 0;
	std::string nodeId;
	int passengerId = -1;
	std::string message;
};
