#pragma once

#include "AssemblyResult.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace assembler {

enum class OutputDiagnosticCode {
    InvalidPath,
    AlreadyExists,
    StageFailed,
    WriteFailed,
    PublishFailed,
    RollbackFailed,
};

struct OutputDiagnostic {
    OutputDiagnosticCode code{};
    std::filesystem::path path;
    std::string message;
};

// Publishes both files without replacing existing files. Staging is local to
// each destination filesystem; publication requires hard-link support.
std::optional<OutputDiagnostic> writeProgramFiles(const AssemblySuccess& program,
                                                  const std::filesystem::path& binaryPath,
                                                  const std::filesystem::path& listingPath);

} // namespace assembler
