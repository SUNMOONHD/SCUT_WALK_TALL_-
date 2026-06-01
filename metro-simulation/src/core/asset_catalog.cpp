#include "asset_catalog.h"

#include <filesystem>
#include <unordered_map>

namespace assets {

std::string iconRoot()
{
	return "resources/icons/";
}

std::string iconPath(const std::string &fileName)
{
	return iconRoot() + fileName;
}

std::string relativeIconPathForReport(const std::string &fileName)
{
	return "../../resources/icons/" + fileName;
}

std::string nodeIconForType(const std::string &nodeType)
{
	static const std::unordered_map<std::string, std::string> map = {
		{"entrance", "node_entrance.svg"},
		{"exit", "node_exit.svg"},
		{"security", "node_security.svg"},
		{"ticket", "node_ticket.svg"},
		{"gate", "node_gate.svg"},
		{"hall", "node_hall.svg"},
		{"corridor", "node_corridor.svg"},
		{"stairs", "node_stairs.svg"},
		{"escalator", "node_escalator.svg"},
		{"platform", "node_platform.svg"},
		{"waiting", "node_waiting.svg"}
	};

	auto it = map.find(nodeType);
	if (it != map.end()) {
		return it->second;
	}
	return "trajectory_dot.svg"; // fallback
}

std::string passengerIconForState(PassengerState state)
{
	switch (state) {
	case PassengerState::Enter: return "passenger_enter.svg";
	case PassengerState::Security: return "passenger_security.svg";
	case PassengerState::Ticket: return "passenger_ticket.svg";
	case PassengerState::Wait: return "passenger_wait.svg";
	case PassengerState::Board: return "passenger_board.svg";
	case PassengerState::Exit: return "passenger_exit.svg";
	case PassengerState::Finished: return "trajectory_end.svg";
	default: return "passenger_walk.svg";
	}
}

std::string eventIconForType(EventType type)
{
	switch (type) {
	case EventType::PassengerArrived: return "trajectory_start.svg";
	case EventType::PassengerExited: return "trajectory_end.svg";
	case EventType::CongestionTriggered: return "alert_congestion.svg";
	case EventType::TimeoutReached: return "alert_congestion.svg";
	default: return "trajectory_dot.svg";
	}
}

std::string lineIconForIndex(int lineIndex)
{
	if (lineIndex == 1) {
		return "line_1.svg";
	} else if (lineIndex == 2) {
		return "line_2.svg";
	}
	return "trajectory_dot.svg";
}

} // namespace assets