#pragma once

#include "MacroDefinition.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace macro {

class MacroTable {
public:
    using Definition = std::shared_ptr<const MacroDefinition>;

    // Copies/moves into an owned immutable version. Existing handles stay valid.
    void define(MacroDefinition definition);
    Definition find(std::string_view name) const;

private:
    std::unordered_map<std::string, Definition> definitions_;
};

} // namespace macro
