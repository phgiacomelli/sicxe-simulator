#include "LinkerCliIO.hpp"
#include "linker/Linker.hpp"
#include "linker/ObjectRecords.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view Usage =
    "Uso: SICXELinker --relocatable MODULE... [-o OUTPUT.obj]\n"
    "     SICXELinker --absolute LOAD_ADDRESS MODULE... [-o OUTPUT.bin]\n"
    "LOAD_ADDRESS aceita decimal ou hexadecimal com prefixo 0x.\n"
    "Opcoes: -o, --output, --, -h, --help. O modo vem primeiro.\n"
    "Saidas padrao no diretorio atual: linked.obj ou linked.bin.\n"
    "O destino precisa estar livre, em diretorio existente que aceite hard links.\n"
    "Retornos: 0 sucesso/ajuda, 1 falha de objeto, ligacao ou arquivo, 2 argumentos invalidos.\n";

std::optional<std::uint32_t> parseLoadAddress(std::string_view text) {
    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint32_t value{};
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value, base);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

int usageError() {
    std::cerr << Usage;
    return 2;
}

} // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    if (arguments.size() == 1 && (arguments.front() == "--help" || arguments.front() == "-h")) {
        std::cout << Usage;
        return 0;
    }
    if (arguments.empty()) {
        return usageError();
    }
    const bool absolute = arguments.front() == "--absolute";
    linker::LinkMode mode = linker::RelocatableLinkMode{};
    std::size_t firstPath = 1;
    if (absolute) {
        if (arguments.size() < 2) {
            return usageError();
        }
        const auto load = parseLoadAddress(arguments[1]);
        if (!load) {
            return usageError();
        }
        mode = linker::AbsoluteLinkMode{*load};
        firstPath = 2;
    } else if (arguments.front() != "--relocatable") {
        return usageError();
    }
    const auto paths =
        cli::parsePaths(std::span<const std::string_view>{arguments}.subspan(firstPath));
    if (!paths || paths->inputs.empty()) {
        return usageError();
    }
    const auto destination = paths->output.value_or(absolute ? "linked.bin" : "linked.obj");
    std::vector<linker::ObjectModule> modules;
    modules.reserve(paths->inputs.size());
    for (const auto& path : paths->inputs) {
        const auto contents = cli::readFile(path, std::cerr);
        if (!contents) {
            return 1;
        }
        auto parsed = linker::parseObject(*contents, path.string());
        if (const auto* failure = std::get_if<linker::ObjectFailure>(&parsed)) {
            cli::printDiagnostics(*failure, path.string(), std::cerr);
            return 1;
        }
        modules.push_back(std::get<linker::ObjectModule>(std::move(parsed)));
    }
    const auto linked = linker::link(modules, mode);
    if (const auto* failure = std::get_if<linker::ObjectFailure>(&linked)) {
        cli::printDiagnostics(*failure, "SICXELinker", std::cerr);
        return 1;
    }
    if (const auto* image = std::get_if<linker::AbsoluteLinkOutput>(&linked)) {
        const std::string_view bytes{reinterpret_cast<const char*>(image->bytes.data()),
                                     image->bytes.size()};
        if (!cli::publishFile(destination, bytes, std::cerr)) {
            return 1;
        }
        std::cout << "Binario: " << destination.string() << " (" << image->bytes.size()
                  << " bytes)\nCarga: 0x" << std::hex << std::uppercase << std::setfill('0')
                  << std::setw(6) << image->loadAddress << "\nPC inicial: 0x" << std::setw(6)
                  << image->initialPc << '\n';
    } else {
        const auto& object = std::get<linker::RelocatableLinkOutput>(linked);
        if (!cli::publishFile(destination, object.records, std::cerr)) {
            return 1;
        }
        std::cout << "Objeto relocavel: " << destination.string()
                  << "\nUse --absolute para gerar uma imagem de carga.\n";
    }
    return 0;
}
