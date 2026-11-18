#pragma once

#include "ObjectModule.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace linker::detail {

std::optional<std::size_t> invalidName(std::string_view value);
std::string normalize(std::string_view name);

// Track all covered bytes, including overlaps, for subsequent M diagnostics.
std::optional<ObjectDiagnostic> validateTextRecord(const TextRecord& text,
                                                   std::vector<bool>& present);
std::optional<ObjectDiagnostic>
validateModificationRecord(const ModificationRecord& modification,
                           const std::vector<bool>& present,
                           std::string_view section,
                           const std::unordered_set<std::string>& references);

} // namespace linker::detail
