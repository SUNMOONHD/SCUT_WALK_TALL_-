#include "utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace utils {

bool loadTextFile(const std::string &path, std::string *content)
{
	if (content == nullptr) {
		return false;
	}

	std::ifstream input(std::filesystem::path(path).c_str(), std::ios::binary);
	if (!input.is_open()) {
		return false;
	}

	std::ostringstream buffer;
	buffer << input.rdbuf();
	*content = buffer.str();
	return true;
}

bool writeTextFile(const std::string &path, const std::string &content)
{
	const std::filesystem::path outputPath(path);
	const std::filesystem::path parentPath = outputPath.parent_path();
	if (!parentPath.empty()) {
		std::error_code errorCode;
		std::filesystem::create_directories(parentPath, errorCode);
	}

	std::ofstream output(outputPath.c_str(), std::ios::binary | std::ios::trunc);
	if (!output.is_open()) {
		return false;
	}

	output << content;
	return true;
}

} // namespace utils
