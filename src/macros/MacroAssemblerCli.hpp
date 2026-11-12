#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace macro {

// Arguments exclude the executable name. Returns 0 for success/help,
// 1 for source/expansion/assembly/I/O errors, 2 for invalid arguments.
int runMacroAssemblerCli(std::span<const std::string_view> arguments,
                         std::ostream& output,
                         std::ostream& errors);

} // namespace macro
