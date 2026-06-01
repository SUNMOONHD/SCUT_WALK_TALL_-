#pragma once

#include <string>

namespace utils {

bool loadTextFile(const std::string &path, std::string *content);
bool writeTextFile(const std::string &path, const std::string &content);

} // namespace utils
