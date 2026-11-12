#include "MacroAssemblerCli.hpp"

#include "ExpandedSourceFile.hpp"
#include "MacroProcessor.hpp"
#include "assembler/Assembler.hpp"
#include "assembler/ListingWriter.hpp"
#include "assembler/OutputFiles.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace macro {
namespace {

constexpr auto Usage =
    "Uso: SICXEMacroAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]\n"
    "     SICXEMacroAssembler --help\n"
    "Opcoes: --output/-o, --listing/-l, --, --help/-h.\n"
    "Publica MASMAPRG.ASM no diretorio de trabalho e substitui a versao anterior\n"
    "somente apos expansao e gravacao completas. O nome nao e configuravel.\n"
    "Falha de leitura/expansao/publicacao preserva a versao anterior; falha de\n"
    "montagem/saida mantem a nova fonte expandida. Fonte e destinos sao protegidos.\n"
    "Binario/listagem usam o nome da fonte original; nao sobrescreve .bin/.lst.\n";

void position(std::ostream& output, const std::string& source, SourcePosition location) {
    output << source << ':' << location.line << ':' << location.column;
}

void callChain(std::ostream& output,
               const std::string& source,
               const std::vector<ExpansionFrame>& calls) {
    for (const auto& call : calls) {
        output << "  macro " << call.macroName << ": definicao ";
        position(output, source, call.definition);
        output << "; chamada ";
        position(output, source, call.invocation);
        output << '\n';
    }
}

} // namespace

int runMacroAssemblerCli(std::span<const std::string_view> arguments,
                         std::ostream& output,
                         std::ostream& errors) {
    if (arguments.size() == 1 && (arguments[0] == "--help" || arguments[0] == "-h")) {
        output << Usage;
        return 0;
    }
    namespace fs = std::filesystem;
    std::optional<fs::path> source;
    std::optional<fs::path> binary;
    std::optional<fs::path> listing;
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
            destination = fs::path{arguments[++index]};
        } else if ((!positionalOnly && argument.starts_with('-')) || argument.empty() || source) {
            errors << "Argumento invalido: " << argument << '\n' << Usage;
            return 2;
        } else {
            source = fs::path{argument};
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
    input.close();
    const auto expanded = expand(text);
    if (const auto* failure = std::get_if<MacroFailure>(&expanded)) {
        for (const auto& diagnostic : failure->diagnostics) {
            position(errors, source->string(), diagnostic.position);
            errors << ": error: " << diagnostic.message << '\n';
            callChain(errors, source->string(), diagnostic.calls);
        }
        return 1;
    }
    const auto& expansion = std::get<MacroSuccess>(expanded);
    const auto publication =
        publishExpandedSource(expansion.expandedText, *source, *binary, *listing);
    if (const auto* failure = std::get_if<ExpandedSourceError>(&publication)) {
        errors << failure->path.string() << ": error: " << failure->message << '\n';
        return 1;
    }
    const auto& expandedPath = std::get<fs::path>(publication);
    const auto assembled = assembler::assemble(expansion.expandedText);
    if (const auto* failure = std::get_if<assembler::AssemblyFailure>(&assembled)) {
        for (const auto& diagnostic : failure->diagnostics) {
            errors << assembler::ListingWriter::diagnostics(
                assembler::AssemblyFailure{{diagnostic}}, expandedPath.string());
            const auto line = diagnostic.position.line;
            const auto* origin = line > 0 && line <= expansion.origins.size()
                                     ? &expansion.origins[line - 1]
                                     : nullptr;
            if (origin && origin->expandedLine == line) {
                errors << "  origem: ";
                position(errors, source->string(), origin->source);
                errors << " (inicio da linha original; colunas podem mudar na expansao)\n";
                callChain(errors, source->string(), origin->calls);
            }
        }
        return 1;
    }
    const auto& program = std::get<assembler::AssemblySuccess>(assembled);
    if (const auto failure = assembler::writeProgramFiles(program, *binary, *listing)) {
        errors << failure->path.string() << ": error: " << failure->message << '\n';
        return 1;
    }
    std::ostringstream message;
    message.imbue(std::locale::classic());
    message << "Fonte expandida: " << expandedPath.string() << "\nBinario: " << binary->string()
            << " (" << program.bytes.size() << " bytes)\nListagem: " << listing->string()
            << "\nCarga: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(6)
            << program.loadAddress << "\nPC inicial: 0x" << std::setw(6) << program.initialPc
            << '\n';
    output << message.str();
    return 0;
}

} // namespace macro
