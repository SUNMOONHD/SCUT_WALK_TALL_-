#include "report_writer.h"

#include "asset_catalog.h"
#include "utils.h"

#include <sstream>
#include <iomanip>

namespace reports {

namespace {

std::string passengerStateToChinese(PassengerState state)
{
	switch (state) {
	case PassengerState::Enter:    return "进站";
	case PassengerState::Security: return "安检";
	case PassengerState::Ticket:   return "购票";
	case PassengerState::Wait:     return "候车";
	case PassengerState::Board:    return "乘车";
	case PassengerState::Exit:     return "出站";
	case PassengerState::Finished: return "完成";
	default:                       return "未知";
	}
}

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

std::string nodeName(const MetroGraph &graph, const std::string &nodeId)
{
	const auto &nodes = graph.nodes();
	auto it = nodes.find(nodeId);
	if (it != nodes.end() && !it->second.name.empty()) {
		return it->second.name;
	}
	return nodeId;
}

std::string formatTime(int totalSeconds)
{
	int h = totalSeconds / 3600;
	int m = (totalSeconds % 3600) / 60;
	int s = totalSeconds % 60;
	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(2) << h << ':'
	    << std::setw(2) << m << ':' << std::setw(2) << s;
	return oss.str();
}

std::string nodeTypeToChinese(const std::string &type)
{
	if (type == "entrance") return "入口";
	if (type == "exit") return "出口";
	if (type == "security") return "安检";
	if (type == "ticket") return "售票";
	if (type == "gate") return "闸机";
	if (type == "hall") return "大厅";
	if (type == "corridor") return "走廊";
	if (type == "platform") return "站台";
	if (type == "waiting") return "候车区";
	if (type == "stairs") return "楼梯";
	if (type == "escalator") return "扶梯";
	return type;
}

std::string floorToString(int floor)
{
	if (floor < 0) return "B" + std::to_string(-floor);
	return "F" + std::to_string(floor);
}

} // namespace

bool writeStep3HtmlReport(const Simulation &sim, const std::string &outputPath, std::string *error)
{
	std::ostringstream html;
	html << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset='UTF-8'>\n<title>地铁仿真报告</title>\n"
		 << "<style>\n"
		 << "body { font-family: 'Microsoft YaHei', Arial, sans-serif; margin: 20px; background: #fafafa; color: #1f1f1f; }\n"
		 << "img.icon { width: 24px; height: 24px; vertical-align: middle; }\n"
		 << "table { border-collapse: collapse; width: 100%; margin-top: 10px; margin-bottom: 20px; }\n"
		 << "th, td { border: 1px solid #d7d7d7; padding: 8px; text-align: left; }\n"
		 << "th { background-color: #f0f0f0; font-weight: bold; }\n"
		 << "h2 { border-bottom: 2px solid #ccc; padding-bottom: 5px; margin-top: 30px; }\n"
		 << "h1 { color: #333; }\n"
		 << ".summary-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; }\n"
		 << ".summary-item { background: #fff; border: 1px solid #e0e0e0; border-radius: 6px; padding: 12px; }\n"
		 << ".summary-item .label { font-size: 12px; color: #888; }\n"
		 << ".summary-item .value { font-size: 20px; font-weight: bold; color: #333; }\n"
		 << ".warn { color: #c43d3d; }\n"
		 << ".ok { color: #4a9c8c; }\n"
		 << "</style>\n</head>\n<body>\n";

	html << "<h1>地铁仿真运行报告</h1>\n";

	const auto &stats = sim.statistics();
	const int totalPassengers = static_cast<int>(sim.passengers().size());
	const int activeCount = totalPassengers - stats.completedPassengers() - stats.timedOutPassengers();
	const double completionRate = totalPassengers > 0
		? (static_cast<double>(stats.completedPassengers()) / totalPassengers * 100.0) : 0.0;

	html << "<h2>仿真概览</h2>\n";
	html << "<div class='summary-grid'>\n";

	html << "<div class='summary-item'><div class='label'>车站名称</div><div class='value'>"
		 << sim.graph().stationName() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>仿真时长</div><div class='value'>"
		 << formatTime(sim.currentTime()) << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>时间步长</div><div class='value'>"
		 << sim.config().timeStep << " 秒</div></div>\n";

	html << "<div class='summary-item'><div class='label'>总生成乘客</div><div class='value'>"
		 << totalPassengers << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>已完成乘客</div><div class='value ok'>"
		 << stats.completedPassengers() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>进行中乘客</div><div class='value'>"
		 << activeCount << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>超时乘客</div><div class='value warn'>"
		 << stats.timedOutPassengers() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>完成率</div><div class='value "
		 << (completionRate >= 80.0 ? "ok" : "warn") << "'>"
		 << std::fixed << std::setprecision(1) << completionRate << "%</div></div>\n";

	double avgTravel = stats.averageTravelTime();
	html << "<div class='summary-item'><div class='label'>平均通行时间</div><div class='value'>"
		 << (avgTravel > 0.0 ? formatTime(static_cast<int>(avgTravel)) : "--") << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>拥堵事件数</div><div class='value "
		 << (stats.congestionEvents() > 0 ? "warn" : "ok") << "'>"
		 << stats.congestionEvents() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>最大排队长度</div><div class='value'>"
		 << stats.maxQueueLength() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>节点总数</div><div class='value'>"
		 << sim.graph().nodeCount() << "</div></div>\n";

	html << "<div class='summary-item'><div class='label'>边总数</div><div class='value'>"
		 << sim.graph().edgeCount() << "</div></div>\n";

	html << "</div>\n";

	html << "<h2>仿真参数</h2>\n<ul>\n";
	html << "<li>高峰时段到达率：" << sim.config().peakLambda << " 人/分钟</li>\n";
	html << "<li>平峰时段到达率：" << sim.config().offpeakLambda << " 人/分钟</li>\n";
	html << "<li>基础速度：" << std::fixed << std::setprecision(1) << sim.config().baseSpeed << " m/s</li>\n";
	html << "<li>最大耐心值：" << sim.config().maxPatience << " 秒</li>\n";
	html << "<li>拥堵阈值：" << (sim.config().congestionThreshold * 100.0) << "%</li>\n";
	html << "<li>安检时间：" << sim.config().processing.securityTime << " 秒</li>\n";
	html << "<li>购票时间：" << sim.config().processing.ticketTimeBase << " 秒</li>\n";
	html << "<li>闸机时间：" << sim.config().processing.gateTime << " 秒</li>\n";
	html << "<li>登车时间：" << sim.config().processing.boardingTime << " 秒</li>\n";
	html << "<li>列车班次间隔：" << sim.config().processing.trainHeadway << " 秒</li>\n";
	if (!sim.config().peakHours.empty()) {
		html << "<li>高峰时段：";
		for (std::size_t i = 0; i < sim.config().peakHours.size(); ++i) {
			if (i > 0) html << "、";
			html << sim.config().peakHours[i].first << ":00-" << sim.config().peakHours[i].second << ":00";
		}
		html << "</li>\n";
	}
	html << "</ul>\n";

	html << "<h2>节点统计</h2>\n<table>\n";
	html << "<tr><th>名称</th><th>类型</th><th>楼层</th><th>容量</th><th>当前人数</th><th>密度</th></tr>\n";
	for (const auto &kv : sim.graph().nodes()) {
		const auto &node = kv.second;
		int count = 0;
		for (const auto &p : sim.passengers()) {
			if (p.state == PassengerState::Finished) continue;
			if (!p.onEdge && p.currentNode == node.id) ++count;
		}
		double density = node.capacity > 0.0 ? (static_cast<double>(count) / node.capacity) : 0.0;
		html << "<tr>"
			 << "<td>" << node.name << "</td>"
			 << "<td>" << nodeTypeToChinese(node.type) << "</td>"
			 << "<td>" << floorToString(node.floor) << "</td>"
			 << "<td>" << std::fixed << std::setprecision(0) << node.capacity << "</td>"
			 << "<td>" << count << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << (density * 100.0) << "%</td>"
			 << "</tr>\n";
	}
	html << "</table>\n";

	html << "<h2>边统计</h2>\n<table>\n";
	html << "<tr><th>起点</th><th>终点</th><th>长度(m)</th><th>宽度(m)</th><th>线路</th><th>换乘时间(s)</th></tr>\n";
	for (const auto &edge : sim.graph().edges()) {
		html << "<tr>"
			 << "<td>" << nodeName(sim.graph(), edge.from) << "</td>"
			 << "<td>" << nodeName(sim.graph(), edge.to) << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << edge.length << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << edge.width << "</td>"
			 << "<td>线路 " << edge.lineIndex << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << edge.transferTime << "</td>"
			 << "</tr>\n";
	}
	html << "</table>\n";

	html << "<h2>乘客日志</h2>\n<table>\n";
	html << "<tr><th>ID</th><th>状态</th><th>当前位置</th><th>目标节点</th><th>耐心值(s)</th><th>已等待(s)</th></tr>\n";
	for (const auto &p : sim.passengers()) {
		html << "<tr>"
			 << "<td>" << p.id << "</td>"
			 << "<td><img class='icon' src='" << assets::relativeIconPathForReport(assets::passengerIconForState(p.state)) << "'> "
			 << passengerStateToChinese(p.state) << "</td>"
			 << "<td>" << (p.onEdge ? ("边上：" + nodeName(sim.graph(), p.edgeFrom) + " → " + nodeName(sim.graph(), p.edgeTo)) : nodeName(sim.graph(), p.currentNode)) << "</td>"
			 << "<td>" << nodeName(sim.graph(), p.targetNode) << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << p.patience << "</td>"
			 << "<td>" << std::fixed << std::setprecision(1) << p.waitedSeconds << "</td>"
			 << "</tr>\n";
	}
	html << "</table>\n";

	html << "<h2>事件日志</h2>\n<table>\n";
	html << "<tr><th>时间</th><th>类型</th><th>乘客 ID</th><th>节点</th><th>消息</th></tr>\n";
	for (const auto &e : sim.events()) {
		html << "<tr>"
			 << "<td>" << formatTime(e.time) << "</td>"
			 << "<td><img class='icon' src='" << assets::relativeIconPathForReport(assets::eventIconForType(e.type)) << "'> "
			 << eventTypeToChinese(e.type) << "</td>"
			 << "<td>" << (e.passengerId > 0 ? std::to_string(e.passengerId) : "-") << "</td>"
			 << "<td>" << (e.nodeId.empty() ? "-" : nodeName(sim.graph(), e.nodeId)) << "</td>"
			 << "<td>" << e.message << "</td>"
			 << "</tr>\n";
	}
	html << "</table>\n";

	html << "</body>\n</html>";

	if (!utils::writeTextFile(outputPath, html.str())) {
		if (error) *error = "写入 HTML 报告失败：" + outputPath;
		return false;
	}
	return true;
}

} // namespace reports