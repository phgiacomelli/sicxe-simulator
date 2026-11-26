#pragma once

#include "ObjectModule.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace linker {

struct AbsoluteLinkMode {
    std::uint32_t loadAddress{};
};

struct RelocatableLinkMode {};

using LinkMode = std::variant<AbsoluteLinkMode, RelocatableLinkMode>;

struct AbsoluteLinkOutput {
    std::vector<std::uint8_t> bytes;
    std::uint32_t loadAddress{};
    std::uint32_t initialPc{};

    bool operator==(const AbsoluteLinkOutput&) const = default;
};

struct RelocatableLinkOutput {
    ObjectModule module;
    std::string records;
};

using LinkResult = std::variant<AbsoluteLinkOutput, RelocatableLinkOutput, ObjectFailure>;

// Modules are placed in input order. A single combined object can be relocated again.
// Results own their data. A failure contains diagnostics only.
LinkResult link(std::span<const ObjectModule> modules, LinkMode mode);

} // namespace linker
