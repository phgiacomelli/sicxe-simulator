#pragma once

#include "ObjectDiagnostic.hpp"
#include "ObjectModule.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace linker {

struct SectionLayout {
    std::string name;
    std::string key;
    std::size_t moduleIndex{};
    std::uint32_t baseOffset{};
    std::uint32_t length{};
    ObjectPosition position;

    constexpr std::uint32_t endOffset() const noexcept {
        return baseOffset + length;
    }

    constexpr bool containsOffset(std::uint32_t relativeOffset) const noexcept {
        return relativeOffset < length;
    }

    constexpr std::uint32_t toGlobalOffset(std::uint32_t relativeOffset) const noexcept {
        return baseOffset + relativeOffset;
    }

    bool operator==(const SectionLayout&) const = default;
};

enum class GlobalSymbolKind {
    Section,
    DefinedSymbol,
};

struct GlobalSymbol {
    std::string name;
    std::string key;
    GlobalSymbolKind kind{GlobalSymbolKind::Section};
    std::size_t moduleIndex{};
    std::string sectionName;
    std::uint32_t sectionBaseOffset{};
    std::uint32_t relativeOffset{};
    std::uint32_t globalOffset{};
    ObjectPosition position;

    constexpr std::uint32_t value() const noexcept {
        return globalOffset;
    }

    constexpr std::uint32_t offset() const noexcept {
        return globalOffset;
    }

    bool operator==(const GlobalSymbol&) const = default;
};

class GlobalSymbolTable {
public:
    static std::string normalizeName(std::string_view name);
    static bool isValidName(std::string_view name);

    std::optional<ObjectDiagnostic> defineSection(std::string_view name,
                                                  std::size_t moduleIndex,
                                                  std::uint32_t baseOffset,
                                                  ObjectPosition position);

    std::optional<ObjectDiagnostic> defineSymbol(std::string_view name,
                                                 std::size_t moduleIndex,
                                                 std::string_view sectionName,
                                                 std::uint32_t sectionBaseOffset,
                                                 std::uint32_t relativeOffset,
                                                 ObjectPosition position);

    std::optional<GlobalSymbol> find(std::string_view name) const;
    std::optional<std::uint32_t> resolve(std::string_view name) const;
    bool contains(std::string_view name) const;
    bool isDefined(std::string_view name) const;

    const std::vector<GlobalSymbol>& symbols() const noexcept;
    std::size_t size() const noexcept;
    bool empty() const noexcept;

    bool operator==(const GlobalSymbolTable&) const = default;

private:
    std::vector<GlobalSymbol> symbols_;
    std::unordered_map<std::string, std::size_t> indexByName_;
};

struct LinkedEntryPoint {
    std::size_t moduleIndex{};
    std::string sectionName;
    std::uint32_t sectionBaseOffset{};
    std::uint32_t relativeOffset{};
    std::uint32_t globalOffset{};
    bool explicitEntry{false};
    ObjectPosition position;

    bool operator==(const LinkedEntryPoint&) const = default;
};

struct LinkPassOneResult {
    std::vector<SectionLayout> sections;
    GlobalSymbolTable symbolTable;
    std::vector<GlobalSymbol> symbols;
    std::uint32_t totalLength{};
    std::uint32_t entryOffset{};
    LinkedEntryPoint entry{};
    std::optional<LinkedEntryPoint> explicitEntry;
    std::vector<ObjectDiagnostic> diagnostics;

    bool succeeded() const noexcept {
        return diagnostics.empty();
    }

    bool hasExplicitEntry() const noexcept {
        return explicitEntry.has_value();
    }

    ObjectFailure failure() const {
        return ObjectFailure{diagnostics};
    }

    std::optional<GlobalSymbol> findSymbol(std::string_view name) const;
    std::optional<std::uint32_t> resolveSymbol(std::string_view name) const;
    std::optional<SectionLayout> findSection(std::string_view name) const;

    constexpr bool fitsInAddressSpace(std::uint32_t loadAddress) const noexcept {
        if (loadAddress >= ObjectAddressSpaceBytes) {
            return false;
        }
        if (totalLength > ObjectAddressSpaceBytes - loadAddress) {
            return false;
        }
        return entryOffset <= (ObjectAddressSpaceBytes - 1U) - loadAddress;
    }

    constexpr std::optional<std::uint32_t>
    resolveInitialPc(std::uint32_t loadAddress) const noexcept {
        if (!fitsInAddressSpace(loadAddress)) {
            return std::nullopt;
        }
        return loadAddress + entryOffset;
    }

    bool operator==(const LinkPassOneResult&) const = default;
};

class LinkPassOne {
public:
    static LinkPassOneResult run(std::span<const ObjectModule> modules);
};

LinkPassOneResult runLinkPassOne(std::span<const ObjectModule> modules);

} // namespace linker
