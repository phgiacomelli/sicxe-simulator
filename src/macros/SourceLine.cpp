#include "SourceLine.hpp"

#include <string_view>

namespace macro {
namespace {

bool equalsAscii(std::string_view text, std::string_view uppercase) {
    if (text.size() != uppercase.size())
        return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        char value = text[index];
        if (value >= 'a' && value <= 'z')
            value = static_cast<char>(value - 'a' + 'A');
        if (value != uppercase[index])
            return false;
    }
    return true;
}

} // namespace

MacroDirective SourceLine::directive() const {
    if (kind == SourceLineKind::Code && operation) {
        if (equalsAscii(operation->text, "MCDEFN"))
            return MacroDirective::DefinitionBegin;
        if (equalsAscii(operation->text, "MCEND"))
            return MacroDirective::DefinitionEnd;
    }
    return MacroDirective::None;
}

} // namespace macro
