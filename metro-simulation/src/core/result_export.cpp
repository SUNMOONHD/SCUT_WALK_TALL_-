#include "result_export.h"

#include "json.hpp"
#include "utils.h"

#include <sstream>

using nlohmann::json;

namespace results {

namespace {

std::string eventTypeToChinese(EventType type)
{
	switch (type) {
	case EventType::PassengerArrived:    return "乘客到达";
	case EventType::PassengerExited:     return "乘客离开";
	case EventType::CongestionTriggered: return "拥堵触发";
	case EventType::TimeoutReached:      return "超时";
	default:                             return "未知";
	}
}

} // namespace

bool exportStep3Results(const Simulation &simulation, const ExportPaths &paths, std::string *error)
{
	json summary;
	summary["station_name"] = simulation.graph().stationName();
	summary["current_time_seconds"] = simulation.currentTime();
	summary["time_step_seconds"] = simulation.timeStep();
	summary["node_count"] = simulation.graph().nodeCount();
	summary["edge_count"] = simulation.graph().edgeCount();
	summary["passenger_count"] = simulation.passengers().size();
	summary["completed_passengers"] = simulation.statistics().completedPassengers();
	summary["timed_out_passengers"] = simulation.statistics().timedOutPassengers();
	summary["average_travel_time_seconds"] = simulation.statistics().averageTravelTime();
	summary["congestion_events"] = simulation.statistics().congestionEvents();
	summary["max_queue_length"] = simulation.statistics().maxQueueLength();

	if (!utils::writeTextFile(paths.summaryJsonPath, summary.dump(2))) {
		if (error != nullptr) {
			*error = "Failed to write summary JSON: " + paths.summaryJsonPath;
		}
		return false;
	}

	std::ostringstream csv;
	csv << "time,type,passenger_id,node_id,message\n";
	for (const auto &event : simulation.events()) {
		csv << event.time << ','
		    << eventTypeToChinese(event.type) << ','
		    << event.passengerId << ','
		    << '"' << event.nodeId << '"' << ','
		    << '"' << event.message << '"' << '\n';
	}

	if (!utils::writeTextFile(paths.eventsCsvPath, csv.str())) {
		if (error != nullptr) {
			*error = "Failed to write event CSV: " + paths.eventsCsvPath;
		}
		return false;
	}

	return true;
}

} // namespace results