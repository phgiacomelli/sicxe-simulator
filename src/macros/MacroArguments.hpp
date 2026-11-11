#pragma once

#include "MacroDefinition.hpp"
#include "MacroDiagnostic.hpp"
#include "SourceLine.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace macro {

enum class MacroFormalKind {
    Label,
    Positional,
};

struct MacroFormal {
    MacroFormalKind kind{MacroFormalKind::Positional};
    SourceField name;
    std::string key;

    bool operator==(const MacroFormal&) const = default;
};

struct MacroPrototype {
    SourceField macroName;
    std::string macroKey;
    std::optional<SourceField> labelFormal;
    std::vector<SourceField> positionalFormals;
    // Ordered formals: labelFormal (if present) followed by positionalFormals.
    std::vector<MacroFormal> formals;

    std::optional<std::size_t> findFormal(std::string_view name) const;

    bool operator==(const MacroPrototype&) const = default;
};

struct MacroCall {
    SourceField macroName;
    std::string macroKey;
    std::optional<SourceField> label;
    std::vector<SourceField> arguments;

    bool operator==(const MacroCall&) const = default;
};

struct GeneratedNameUse {
    SourceField token; // Byte position in the containing line (or relative to a bound value).
    std::size_t serial;
    bool operator==(const GeneratedNameUse&) const = default;
};

struct BoundMacroArgument {
    MacroFormal formal;
    std::string value;
    // Physical position in the call when present; nullopt when omitted.
    std::optional<SourcePosition> position;
    // Occurrences relative to value, with columns starting at 1.
    std::vector<GeneratedNameUse> generatedNames{};

    bool operator==(const BoundMacroArgument&) const = default;
};

struct BoundMacroArguments {
    SourceField macroName;
    std::optional<std::string> labelValue;
    std::vector<std::string> positionalValues;
    // Ordered values matching MacroPrototype::formals.
    std::vector<std::string> values;
    std::vector<BoundMacroArgument> arguments;

    const BoundMacroArgument* find(std::string_view formalName) const;

    bool operator==(const BoundMacroArguments&) const = default;
};

using MacroPrototypeResult = std::variant<MacroPrototype, MacroDiagnostic>;
using MacroCallResult = std::variant<MacroCall, MacroDiagnostic>;
using MacroArgumentBindResult = std::variant<BoundMacroArguments, MacroDiagnostic>;

class MacroArguments {
public:
    static MacroPrototypeResult parsePrototype(const SourceLine& line);
    static MacroPrototypeResult parsePrototype(const MacroDefinition& definition);

    // The engine selects an active macro first; this does not classify SIC/XE
    // operations or check the assembler's reserved names.
    static MacroCallResult parseCall(const SourceLine& line);

    static MacroArgumentBindResult bind(const MacroPrototype& prototype, const MacroCall& call);
    static MacroArgumentBindResult bind(const MacroPrototype& prototype,
                                        const SourceLine& callLine);
    static MacroArgumentBindResult bind(const SourceLine& prototypeLine,
                                        const SourceLine& callLine);
    static MacroArgumentBindResult bind(const MacroDefinition& definition,
                                        const SourceLine& callLine);
};

} // namespace macro
