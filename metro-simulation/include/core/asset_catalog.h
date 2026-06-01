#pragma once

#include "event.h"
#include "passenger.h"

#include <string>

namespace assets {

std::string iconRoot();
std::string iconPath(const std::string &fileName);
std::string relativeIconPathForReport(const std::string &fileName);

std::string nodeIconForType(const std::string &nodeType);
std::string passengerIconForState(PassengerState state);
std::string eventIconForType(EventType type);
std::string lineIconForIndex(int lineIndex);

} // namespace assets