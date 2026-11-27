#include "LinkerCliIO.hpp"
#include "assembler/ListingWriter.hpp"
#include "assembler/ObjectAssembler.hpp"
#include "linker/ObjectRecords.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view Usage =
    "Uso: SICXEObjectAssembler SOURCE [-o MODULE.obj]\n"
    "Opcoes: -o, --output, --, -h, --help.\n"
    "Saida padrao: SOURCE com extensao .obj. A saida precisa estar livre.\n"
    "O diretorio de destino deve existir e aceitar hard links.\n"
    "Retornos: 0 sucesso/ajuda, 1 falha de montagem ou de arquivo, 2 argumentos invalidos.\n";

} // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    if (arguments.size() == 1 && (arguments.front() == "--help" || arguments.front() == "-h")) {
        std::cout << Usage;
        return 0;
    }
    const auto paths = cli::parsePaths(arguments);
    if (!paths || paths->inputs.size() != 1) {
        std::cerr << Usage;
        return 2;
    }
    const auto& source = paths->inputs.front();
    auto destination = paths->output.value_or(source);
    if (!paths->output) {
        destination.replace_extension(".obj");
    }
    const auto contents = cli::readFile(source, std::cerr);
    if (!contents) {
        return 1;
    }
    const auto object = assembler::assembleObject(*contents);
    if (const auto* failure = std::get_if<assembler::AssemblyFailure>(&object)) {
        std::cerr << assembler::ListingWriter::diagnostics(*failure, source.string());
        return 1;
    }
    const auto rendered = linker::renderObject(std::get<linker::ObjectModule>(object));
    if (const auto* failure = std::get_if<linker::ObjectFailure>(&rendered)) {
        cli::printDiagnostics(*failure, source.string(), std::cerr);
        return 1;
    }
    if (!cli::publishFile(destination, std::get<std::string>(rendered), std::cerr)) {
        return 1;
    }
    std::cout << "Objeto: " << destination.string() << '\n';
    return 0;
}
