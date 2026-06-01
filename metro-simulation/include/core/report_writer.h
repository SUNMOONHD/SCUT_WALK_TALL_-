#pragma once

#include "simulation.h"

#include <string>

namespace reports {

bool writeStep3HtmlReport(const Simulation &simulation, const std::string &outputPath, std::string *error);

} // namespace reports