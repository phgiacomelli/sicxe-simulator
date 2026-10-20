#include "OutputFiles.hpp"

#include "ListingWriter.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <variant>
#include <vector>

namespace assembler {
namespace {
namespace fs = std::filesystem;

OutputDiagnostic failure(OutputDiagnosticCode code,
                         const fs::path& path,
                         const std::string& message,
                         std::error_code error = {}) {
    return {code, path, message + (error ? ": " + error.message() : "")};
}

std::optional<OutputDiagnostic> available(const fs::path& path) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory)
        return failure(
            OutputDiagnosticCode::InvalidPath, path, "Nao foi possivel consultar a saida", error);
    if (!error && status.type() != fs::file_type::not_found)
        return failure(OutputDiagnosticCode::AlreadyExists,
                       path,
                       "Saida ja existe; escolha outro caminho (nenhum arquivo foi substituido)");
    return std::nullopt;
}

struct StagingDirectories {
    std::vector<fs::path> paths;
    ~StagingDirectories() {
        for (const auto& path : paths) {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    }
};

using StageResult = std::variant<fs::path, OutputDiagnostic>;

StageResult destinationPath(const fs::path& path) {
    if (path.empty())
        return failure(OutputDiagnosticCode::InvalidPath, path, "Caminho de saida vazio");
    std::error_code error;
    const auto absolute = fs::absolute(path, error);
    if (error)
        return failure(OutputDiagnosticCode::InvalidPath, path, "Caminho de saida invalido", error);
    // Resolve parent aliases, but retain the leaf so existing symlinks are
    // treated as occupied destinations rather than followed for publication.
    const auto parent = fs::weakly_canonical(absolute.parent_path(), error);
    if (error)
        return failure(
            OutputDiagnosticCode::InvalidPath, path, "Diretorio de saida invalido", error);
    return (parent / absolute.filename()).lexically_normal();
}

StageResult stage(const fs::path& destination, StagingDirectories& cleanup) {
    static std::atomic<unsigned int> sequence{};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto path = destination.parent_path() / (".sicxe-output-" + std::to_string(stamp) +
                                                       "-" + std::to_string(sequence++));
        std::error_code error;
        if (fs::create_directory(path, error)) {
            cleanup.paths.push_back(path);
            return path / "content";
        }
        if (error && error != std::errc::file_exists)
            return failure(OutputDiagnosticCode::StageFailed,
                           destination,
                           "Nao foi possivel preparar a saida",
                           error);
    }
    return failure(OutputDiagnosticCode::StageFailed,
                   destination,
                   "Nao foi possivel criar uma area temporaria exclusiva");
}

bool write(const fs::path& path, const char* data, std::size_t size) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output.write(data, static_cast<std::streamsize>(size));
    output.flush();
    const bool written = output.good();
    output.close();
    return written && !output.fail();
}

} // namespace

std::optional<OutputDiagnostic> writeProgramFiles(const AssemblySuccess& program,
                                                  const fs::path& binaryPath,
                                                  const fs::path& listingPath) {
    if (program.bytes.empty())
        return failure(OutputDiagnosticCode::WriteFailed, binaryPath, "Imagem binaria vazia");
    const auto resolvedBinary = destinationPath(binaryPath);
    if (const auto* diagnostic = std::get_if<OutputDiagnostic>(&resolvedBinary))
        return *diagnostic;
    const auto resolvedListing = destinationPath(listingPath);
    if (const auto* diagnostic = std::get_if<OutputDiagnostic>(&resolvedListing))
        return *diagnostic;
    const auto& binary = std::get<fs::path>(resolvedBinary);
    const auto& listing = std::get<fs::path>(resolvedListing);
    if (binary == listing)
        return failure(OutputDiagnosticCode::InvalidPath,
                       listingPath,
                       "Saidas precisam de caminhos distintos");
    if (auto diagnostic = available(binary))
        return diagnostic;
    if (auto diagnostic = available(listing))
        return diagnostic;

    StagingDirectories cleanup;
    const auto binaryStage = stage(binary, cleanup);
    if (const auto* diagnostic = std::get_if<OutputDiagnostic>(&binaryStage))
        return *diagnostic;
    const auto listingStage = stage(listing, cleanup);
    if (const auto* diagnostic = std::get_if<OutputDiagnostic>(&listingStage))
        return *diagnostic;
    const auto listingText = ListingWriter::render(program);
    if (!write(std::get<fs::path>(binaryStage),
               reinterpret_cast<const char*>(program.bytes.data()),
               program.bytes.size()))
        return failure(
            OutputDiagnosticCode::WriteFailed, binary, "Falha ao gravar todos os bytes binarios");
    if (!write(std::get<fs::path>(listingStage), listingText.data(), listingText.size()))
        return failure(
            OutputDiagnosticCode::WriteFailed, listing, "Falha ao gravar toda a listagem");

    // A hard link publishes a complete file atomically and fails if the target
    // already exists, including a concurrent creator or a dangling symlink.
    std::error_code error;
    fs::create_hard_link(std::get<fs::path>(binaryStage), binary, error);
    if (error)
        return failure(
            OutputDiagnosticCode::PublishFailed,
            binary,
            "Falha ao publicar binario sem sobrescrita (filesystem deve suportar hard links)",
            error);
    fs::create_hard_link(std::get<fs::path>(listingStage), listing, error);
    if (error) {
        const auto publishError = error;
        std::error_code rollbackError;
        fs::remove(binary, rollbackError);
        if (rollbackError)
            return failure(OutputDiagnosticCode::RollbackFailed,
                           binary,
                           "Listagem nao publicada e binario precisa ser removido manualmente",
                           rollbackError);
        return failure(OutputDiagnosticCode::PublishFailed,
                       listing,
                       "Listagem nao publicada; binario desfeito",
                       publishError);
    }
    return std::nullopt;
}

} // namespace assembler
