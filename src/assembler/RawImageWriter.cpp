#include "RawImageWriter.hpp"

#include "AddressLimits.hpp"

#include <algorithm>

namespace assembler {

RawImageResult RawImageWriter::build(std::uint32_t startAddress,
                                     std::size_t length,
                                     std::span<const AssemblyListingLine> listing) {
    if (length == 0)
        return AssemblyDiagnostic{
            AssemblyDiagnosticCode::EmptyProgram, {1, 1}, "Programa nao emite nem reserva bytes."};
    if (startAddress > MaxAddressableByte || length > AddressSpaceBytes - startAddress)
        return AssemblyDiagnostic{AssemblyDiagnosticCode::ValueOutOfRange,
                                  {1, 1},
                                  "Imagem excede o espaco enderecavel de 20 bits."};

    std::size_t offset = 0;
    for (const auto& line : listing) {
        const bool hasBytes = !line.bytes.empty();
        const bool hasReservation = line.reservedBytes != 0;
        const auto size = hasReservation ? line.reservedBytes : line.bytes.size();
        if ((hasBytes && hasReservation) || (!line.address && size != 0) ||
            (line.address && *line.address != startAddress + offset) || size > length - offset) {
            return AssemblyDiagnostic{AssemblyDiagnosticCode::LayoutMismatch,
                                      {line.line, 1},
                                      "Bytes/reserva divergem do layout contiguo da passagem 1."};
        }
        offset += size;
    }
    if (offset != length)
        return AssemblyDiagnostic{AssemblyDiagnosticCode::LayoutMismatch,
                                  {1, 1},
                                  "Extensao codificada diverge do comprimento da passagem 1."};

    std::vector<std::uint8_t> image(length, 0);
    for (const auto& line : listing) {
        if (!line.bytes.empty())
            std::copy(line.bytes.begin(),
                      line.bytes.end(),
                      image.begin() + (*line.address - startAddress));
    }
    return image;
}

} // namespace assembler
