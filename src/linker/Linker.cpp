#include "Linker.hpp"

#include "LinkPassOne.hpp"
#include "LinkPassTwo.hpp"
#include "ObjectRecords.hpp"
#include "RelocationField.hpp"

#include <optional>
#include <utility>

namespace linker {
namespace {

// Compare against the permitted delta before multiplying, including when the
// load term brings a negative or oversized pre-load value back into range.
std::optional<std::uint32_t> loadedAddress(const LinkModificationResidual& field,
                                           std::uint32_t load) {
    const auto maxAddress = relocationMask(RelocationWidth::Address);
    const auto before = field.valueBeforeLoad;
    const auto coefficient = field.loadAddressCoefficient;
    const auto base = static_cast<std::uint64_t>(before);
    auto value = base;
    if (load != 0 && coefficient < 0) {
        const auto magnitude = 0U - static_cast<std::uint64_t>(coefficient);
        if (before < 0 || magnitude > base / load) {
            return std::nullopt;
        }
        value -= magnitude * load;
    } else if (load != 0 && coefficient > 0) {
        const auto magnitude = static_cast<std::uint64_t>(coefficient);
        // Unsigned subtraction also represents the distance from a negative base.
        if (before > maxAddress || magnitude > (maxAddress - base) / load) {
            return std::nullopt;
        }
        value += magnitude * load;
    }
    if (value > maxAddress) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

ObjectFailure outOfRange(const ObjectPosition& position, const char* message) {
    return {{{ObjectDiagnosticCode::ValueOutOfRange, position, message}}};
}

LinkResult makeRelocatable(const LinkPassTwoIntermediate& intermediate,
                           const LinkPassOneResult& layout) {
    const auto& first = layout.sections.front();
    ObjectModule module;
    module.header = {first.name, intermediate.totalLength, first.position};
    module.entry.position = layout.entry.position;
    if (layout.hasExplicitEntry()) {
        module.entry.offset = intermediate.entryOffset;
    }

    for (const auto& field : intermediate.residuals) {
        if (field.halfBytes == static_cast<std::uint8_t>(RelocationWidth::Address) &&
            !loadedAddress(field, 0)) {
            return outOfRange(field.position,
                              "Valor de formato 4 antes da carga nao cabe no objeto relocavel.");
        }
        const auto coefficient = field.loadAddressCoefficient;
        const auto sign = coefficient < 0 ? ModificationSign::Subtract : ModificationSign::Add;
        const auto step = coefficient < 0 ? 1 : -1;
        for (auto remaining = coefficient; remaining != 0; remaining += step) {
            module.modifications.push_back(
                {field.offset, field.halfBytes, sign, first.name, field.position});
        }
    }

    std::uint32_t offset = 0;
    while (offset < intermediate.totalLength) {
        if (!intermediate.text[offset]) {
            ++offset;
            continue;
        }
        TextRecord record{offset, {}, first.position};
        while (offset < intermediate.totalLength && intermediate.text[offset] &&
               record.bytes.size() < MaxTextRecordBytes) {
            record.bytes.push_back(*intermediate.text[offset++]);
        }
        module.text.push_back(std::move(record));
    }

    auto records = renderObject(module);
    if (auto* failure = std::get_if<ObjectFailure>(&records)) {
        return std::move(*failure);
    }
    return RelocatableLinkOutput{std::move(module), std::get<std::string>(std::move(records))};
}

} // namespace

LinkResult link(std::span<const ObjectModule> modules, LinkMode mode) {
    const auto layout = LinkPassOne::run(modules);
    if (!layout.succeeded()) {
        return layout.failure();
    }
    const auto linked = LinkPassTwo::run(modules, layout);
    if (!linked.succeeded()) {
        return linked.failure();
    }

    const auto* absolute = std::get_if<AbsoluteLinkMode>(&mode);
    if (!absolute) {
        return makeRelocatable(*linked.intermediate, layout);
    }
    const auto pc = layout.resolveInitialPc(absolute->loadAddress);
    if (layout.totalLength == 0) {
        return outOfRange(layout.sections.front().position, "A imagem absoluta esta vazia.");
    }
    if (!pc) {
        return outOfRange(layout.entry.position,
                          "Imagem ou ponto de entrada fora do espaco de carga.");
    }

    AbsoluteLinkOutput output{{}, absolute->loadAddress, *pc};
    output.bytes.reserve(layout.totalLength);
    for (const auto byte : linked.intermediate->text) {
        output.bytes.push_back(byte.value_or(0));
    }
    for (const auto& field : linked.intermediate->residuals) {
        const auto width = static_cast<RelocationWidth>(field.halfBytes);
        std::uint32_t value;
        if (width == RelocationWidth::Address) {
            const auto address = loadedAddress(field, absolute->loadAddress);
            if (!address) {
                return outOfRange(field.position, "Endereco de formato 4 fora dos 20 bits.");
            }
            value = *address;
        } else {
            // Unsigned arithmetic preserves the low 24 bits even for negative terms.
            const auto sum =
                static_cast<std::uint64_t>(field.valueBeforeLoad) +
                static_cast<std::uint64_t>(field.loadAddressCoefficient) * absolute->loadAddress;
            value = static_cast<std::uint32_t>(sum & relocationMask(width));
        }
        writeRelocationField(
            std::span<std::uint8_t, 3>{output.bytes.data() + field.offset, 3}, width, value);
    }
    return output;
}

} // namespace linker
