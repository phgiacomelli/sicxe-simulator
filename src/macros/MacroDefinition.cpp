#include "MacroDefinition.hpp"

#include <utility>

namespace macro {

MacroDefinition::MacroDefinition(SourceLine begin,
                                 SourceLine prototype,
                                 std::vector<SourceLine> body,
                                 SourceLine end)
    : begin_(std::move(begin)), prototype_(std::move(prototype)), body_(std::move(body)),
      end_(std::move(end)) {
}

const SourceLine& MacroDefinition::begin() const noexcept {
    return begin_;
}

const SourceLine& MacroDefinition::prototype() const noexcept {
    return prototype_;
}

const std::vector<SourceLine>& MacroDefinition::body() const noexcept {
    return body_;
}

const SourceLine& MacroDefinition::end() const noexcept {
    return end_;
}

const SourceField& MacroDefinition::name() const noexcept {
    return *prototype_.operation;
}

} // namespace macro
