#include "LinkerCliIO.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <ostream>
#include <system_error>

namespace cli {
namespace {
namespace fs = std::filesystem;

bool fileError(std::ostream& errors,
               const fs::path& path,
               std::string_view message,
               const std::error_code& error = {}) {
    errors << path.string() << ": error: " << message;
    if (error) {
        errors << ": " << error.message();
    }
    errors << '\n';
    return false;
}

struct StagedFile {
    fs::path directory;
    fs::path file;

    ~StagedFile() {
        std::error_code error;
        fs::remove(file, error);
        fs::remove(directory, error);
    }
};

} // namespace

std::optional<Paths> parsePaths(std::span<const std::string_view> arguments) {
    Paths paths;
    bool positionalOnly = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        if (!positionalOnly && argument == "--") {
            positionalOnly = true;
        } else if (!positionalOnly && (argument == "-o" || argument == "--output")) {
            if (paths.output || ++index == arguments.size() || arguments[index].empty() ||
                arguments[index].starts_with('-')) {
                return std::nullopt;
            }
            paths.output.emplace(arguments[index]);
        } else if (argument.empty() || (!positionalOnly && argument.starts_with('-'))) {
            return std::nullopt;
        } else {
            paths.inputs.emplace_back(argument);
        }
    }
    return paths;
}

std::optional<std::string> readFile(const fs::path& path, std::ostream& errors) {
    std::error_code error;
    if (!fs::is_regular_file(path, error)) {
        fileError(errors, path, "Entrada precisa ser um arquivo regular", error);
        return std::nullopt;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fileError(errors, path, "Nao foi possivel abrir a entrada");
        return std::nullopt;
    }
    std::string contents;
    std::array<char, 4096> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() != 0) {
        contents.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (!input.eof() || input.bad()) {
        fileError(errors, path, "Nao foi possivel ler a entrada completa");
        return std::nullopt;
    }
    return contents;
}

bool publishFile(const fs::path& path, std::string_view contents, std::ostream& errors) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        return fileError(errors, path, "Nao foi possivel consultar o destino", error);
    }
    // Occupied destinations include input aliases and dangling symlinks.
    if (!error && status.type() != fs::file_type::not_found) {
        return fileError(errors, path, "Saida ja existe");
    }

    const auto parent = path.has_parent_path() ? path.parent_path() : fs::path{"."};
    static std::atomic<unsigned int> sequence{};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto directory =
            parent / (".sicxe-stage-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
        if (!fs::create_directory(directory, error)) {
            if (!error || error == std::errc::file_exists) {
                continue;
            }
            return fileError(errors, path, "Nao foi possivel preparar a saida", error);
        }
        const StagedFile staged{directory, directory / "content"};
        std::ofstream output(staged.file, std::ios::binary);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            return fileError(errors, path, "Nao foi possivel gravar a saida completa");
        }
        // Publish a complete file without replacing a concurrent destination.
        fs::create_hard_link(staged.file, path, error);
        if (error) {
            return fileError(
                errors, path, "Nao foi possivel publicar a saida sem sobrescrita", error);
        }
        return true;
    }
    return fileError(errors, path, "Nao foi possivel criar um diretorio temporario exclusivo");
}

void printDiagnostics(const linker::ObjectFailure& failure,
                      std::string_view fallbackName,
                      std::ostream& errors) {
    for (const auto& diagnostic : failure.diagnostics) {
        const auto& where = diagnostic.position;
        errors << (where.file.empty() ? fallbackName : std::string_view{where.file}) << ':'
               << where.record << ':' << where.column << ": error: " << diagnostic.message << '\n';
    }
}

} // namespace cli
