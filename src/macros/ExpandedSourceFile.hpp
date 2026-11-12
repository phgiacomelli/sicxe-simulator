#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace macro {

struct ExpandedSourceError {
    std::filesystem::path path;
    std::string message;
};

using ExpandedSourceResult = std::variant<std::filesystem::path, ExpandedSourceError>;

// Atomically replaces the fixed MASMAPRG.ASM in the working directory after
// writing/closing a complete temporary file. Never overwrites the source or
// aliases of the binary/listing; refuses symlinks and non-regular destinations.
ExpandedSourceResult publishExpandedSource(std::string_view text,
                                           const std::filesystem::path& source,
                                           const std::filesystem::path& binary,
                                           const std::filesystem::path& listing);

} // namespace macro
