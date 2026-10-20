#include "ListingWriter.hpp"

#include <iomanip>
#include <locale>
#include <sstream>

namespace assembler {
namespace {

void address(std::ostream& output, std::uint32_t value) {
    output << std::hex << std::uppercase << std::setfill('0') << std::setw(6) << value << std::dec;
}

} // namespace

std::string ListingWriter::render(const AssemblySuccess& program) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "LOAD ";
    address(output, program.loadAddress);
    output << "  ENTRY ";
    address(output, program.initialPc);
    output << "  SIZE " << program.bytes.size() << " bytes\n"
           << "LINE\tADDRESS\tOBJECT/RESERVE\tSOURCE\n";
    for (const auto& line : program.listing) {
        output << line.line << '\t';
        if (line.address)
            address(output, *line.address);
        output << '\t';
        if (line.reservedBytes != 0) {
            output << "RESERVE " << line.reservedBytes;
        } else {
            for (std::size_t index = 0; index < line.bytes.size(); ++index) {
                if (index != 0)
                    output << ' ';
                output << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                       << static_cast<unsigned int>(line.bytes[index]) << std::dec;
            }
        }
        output << '\t' << line.source << '\n';
    }
    output << "\nSYMBOL\tADDRESS\tDEFINITION\n";
    for (const auto& symbol : program.symbols) {
        output << symbol.name << '\t';
        address(output, symbol.address);
        output << '\t' << symbol.definition.line << ':' << symbol.definition.column << '\n';
    }
    return output.str();
}

std::string ListingWriter::diagnostics(const AssemblyFailure& failure,
                                       std::string_view sourceName) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    for (const auto& diagnostic : failure.diagnostics)
        output << sourceName << ':' << diagnostic.position.line << ':' << diagnostic.position.column
               << ": error: " << diagnostic.message << '\n';
    return output.str();
}

} // namespace assembler
