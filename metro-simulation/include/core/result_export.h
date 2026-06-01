#pragma once

#include "simulation.h"

#include <string>

namespace results {

struct ExportPaths {
	std::string summaryJsonPath;
	std::string eventsCsvPath;
};

bool exportStep3Results(const Simulation &simulation, const ExportPaths &paths, std::string *error);

} // namespace results