#include "MacroTable.hpp"

#include "assembler/SymbolTable.hpp"

#include <utility>

namespace macro {

void MacroTable::define(MacroDefinition definition) {
    auto key = assembler::SymbolTable::normalizeName(definition.name().text);
    definitions_.insert_or_assign(std::move(key),
                                  std::make_shared<const MacroDefinition>(std::move(definition)));
}

MacroTable::Definition MacroTable::find(std::string_view name) const {
    const auto found = definitions_.find(assembler::SymbolTable::normalizeName(name));
    return found == definitions_.end() ? nullptr : found->second;
}

} // namespace macro
