#pragma once

#include <cstddef>

namespace macro {

struct SourcePosition {
    // One-based physical line and byte column; a tab counts as one byte.
    std::size_t line{1};
    std::size_t column{1};

    bool operator==(const SourcePosition&) const = default;
};

} // namespace macro
