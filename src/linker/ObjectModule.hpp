#pragma once

#include "ObjectDiagnostic.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace linker {

inline constexpr std::uint32_t ObjectAddressSpaceBytes = 0x100000U;
inline constexpr std::size_t MaxTextRecordBytes = 30;

struct ObjectHeader {
    std::string name;
    std::uint32_t length{};
    ObjectPosition position;

    bool operator==(const ObjectHeader&) const = default;
};

struct DefinedSymbol {
    std::string name;
    std::uint32_t offset{};
    ObjectPosition position;

    bool operator==(const DefinedSymbol&) const = default;
};

struct ReferencedSymbol {
    std::string name;
    ObjectPosition position;

    bool operator==(const ReferencedSymbol&) const = default;
};

struct TextRecord {
    std::uint32_t offset{};
    std::vector<std::uint8_t> bytes;
    ObjectPosition position;

    bool operator==(const TextRecord&) const = default;
};

enum class ModificationSign {
    Add,
    Subtract,
};

struct ModificationRecord {
    std::uint32_t offset{};
    std::uint8_t halfBytes{};
    ModificationSign sign{ModificationSign::Add};
    std::string symbol;
    ObjectPosition position;

    bool operator==(const ModificationRecord&) const = default;
};

struct EntryRecord {
    std::optional<std::uint32_t> offset;
    ObjectPosition position;

    bool operator==(const EntryRecord&) const = default;
};

// One section, origin zero. Offsets are section-relative, not load addresses.
// Text contains only initialized bytes; section length includes reserved holes.
struct ObjectModule {
    ObjectHeader header;
    std::vector<DefinedSymbol> definitions;
    std::vector<ReferencedSymbol> references;
    std::vector<TextRecord> text;
    std::vector<ModificationRecord> modifications;
    EntryRecord entry;

    bool operator==(const ObjectModule&) const = default;
};

} // namespace linker
