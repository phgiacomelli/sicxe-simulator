#pragma once

#include "linker/ObjectDiagnostic.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

struct Paths {
    std::vector<std::filesystem::path> inputs;
    std::optional<std::filesystem::path> output;
};

std::optional<Paths> parsePaths(std::span<const std::string_view> arguments);
std::optional<std::string> readFile(const std::filesystem::path& path, std::ostream& errors);
bool publishFile(const std::filesystem::path& path,
                 std::string_view contents,
                 std::ostream& errors);
void printDiagnostics(const linker::ObjectFailure& failure,
                      std::string_view fallbackName,
                      std::ostream& errors);

} // namespace cli
