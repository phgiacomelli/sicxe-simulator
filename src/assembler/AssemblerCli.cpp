#include "AssemblerCli.hpp"

#include "Assembler.hpp"
#include "ListingWriter.hpp"
#include "OutputFiles.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>

namespace assembler {
namespace {

constexpr auto Usage =
    "Uso: SICXEAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]\n"
    "     SICXEAssembler --help\n"
    "Saidas padrao: SOURCE com extensoes .bin e .lst; nao sobrescreve arquivos.\n";

} // namespace

int runAssemblerCli(std::span<const std::string_view> arguments,
                    std::ostream& output,
                    std::ostream& errors) {
    if (arguments.size() == 1 && (arguments[0] == "--help" || arguments[0] == "-h")) {
        output << Usage;
        return 0;
    }
    std::optional<std::filesystem::path> source;
    std::optional<std::filesystem::path> binary;
    std::optional<std::filesystem::path> listing;
    bool positionalOnly = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        if (!positionalOnly && argument == "--") {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly && (argument == "-o" || argument == "--output" || argument == "-l" ||
                                argument == "--listing")) {
            auto& destination = (argument == "-o" || argument == "--output") ? binary : listing;
            if (destination || index + 1 == arguments.size() || arguments[index + 1].empty()) {
                errors << "Opcao repetida ou sem caminho: " << argument << '\n' << Usage;
                return 2;
            }
            destination = std::filesystem::path{arguments[++index]};
        } else if ((!positionalOnly && argument.starts_with('-')) || argument.empty() || source) {
            errors << "Argumento invalido: " << argument << '\n' << Usage;
            return 2;
        } else {
            source = std::filesystem::path{argument};
        }
    }
    if (!source) {
        errors << Usage;
        return 2;
    }
    if (!binary) {
        binary = *source;
        binary->replace_extension(".bin");
    }
    if (!listing) {
        listing = *source;
        listing->replace_extension(".lst");
    }
    std::ifstream input(*source, std::ios::binary);
    if (!input) {
        errors << source->string() << ": error: nao foi possivel abrir a fonte.\n";
        return 1;
    }
    std::string text;
    std::array<char, 4096> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() != 0)
        text.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    if (!input.eof() || input.bad()) {
        errors << source->string() << ": error: falha ao ler a fonte completa.\n";
        return 1;
    }
    const auto result = assemble(text);
    if (const auto* failure = std::get_if<AssemblyFailure>(&result)) {
        errors << ListingWriter::diagnostics(*failure, source->string());
        return 1;
    }
    const auto& program = std::get<AssemblySuccess>(result);
    if (const auto failure = writeProgramFiles(program, *binary, *listing)) {
        errors << failure->path.string() << ": error: " << failure->message << '\n';
        return 1;
    }
    std::ostringstream message;
    message.imbue(std::locale::classic());
    message << "Binario: " << binary->string() << " (" << program.bytes.size() << " bytes)\n"
            << "Listagem: " << listing->string() << "\nCarga: 0x" << std::hex << std::uppercase
            << std::setfill('0') << std::setw(6) << program.loadAddress << "\nPC inicial: 0x"
            << std::setw(6) << program.initialPc << '\n';
    output << message.str();
    return 0;
}

} // namespace assembler
