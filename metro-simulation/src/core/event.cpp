#include "event.h"

#include <utility>

Event::Event(EventType eventType, int eventTime, std::string nodeIdValue, int passengerIdValue, std::string eventMessage)
		: type(eventType),
			time(eventTime),
			nodeId(std::move(nodeIdValue)),
			passengerId(passengerIdValue),
			message(std::move(eventMessage))
{
}
