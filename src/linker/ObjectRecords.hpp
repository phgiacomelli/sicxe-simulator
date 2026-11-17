#pragma once

#include "ObjectDiagnostic.hpp"
#include "ObjectModule.hpp"

#include <string>
#include <string_view>
#include <variant>

namespace linker {

using ObjectParseResult = std::variant<ObjectModule, ObjectFailure>;
using ObjectRenderResult = std::variant<std::string, ObjectFailure>;

// Pure fixed-width record parsing; source name and every result field are owned.
// Returns the first detected failure, never a partially validated module.
ObjectParseResult parseObject(std::string_view records, std::string_view fileName = {});
// Validates values too. Emits H/D/R/T/M/E, named M, uppercase hex and LF.
// Does not resolve references or apply modifications to the text bytes.
ObjectRenderResult renderObject(const ObjectModule& module);

} // namespace linker
