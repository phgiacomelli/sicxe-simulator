#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace assembler {

// Arguments exclude the executable name. Exit codes: 0 success/help,
// 1 source/assembly/output error, 2 invalid command line.
int runAssemblerCli(std::span<const std::string_view> arguments,
                    std::ostream& output,
                    std::ostream& errors);

} // namespace assembler
