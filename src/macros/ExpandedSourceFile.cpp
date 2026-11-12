#include "ExpandedSourceFile.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <optional>
#include <utility>

namespace macro {
namespace {
namespace fs = std::filesystem;

ExpandedSourceError failure(const fs::path& path, std::string message, std::error_code error = {}) {
    if (error)
        message += ": " + error.message();
    return {path, std::move(message)};
}

struct StagingDirectory {
    fs::path path;
    ~StagingDirectory() {
        if (!path.empty()) {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    }
};

std::optional<ExpandedSourceError> protect(const fs::path& destination, const fs::path& other) {
    std::error_code error;
    auto absolute = fs::absolute(other, error);
    if (error)
        return failure(other, "Caminho protegido invalido", error);
    for (int links = 0; links < 40; ++links) {
        const auto canonical = fs::weakly_canonical(absolute, error);
        if (error)
            return failure(other, "Nao foi possivel verificar o caminho protegido", error);
        if (canonical == destination)
            return failure(other, "Fonte e saidas nao podem apontar para MASMAPRG.ASM");
        const bool same = fs::equivalent(destination, canonical, error);
        if (!error && same)
            return failure(other, "Fonte e saidas nao podem ser aliases de MASMAPRG.ASM");
        if (error && error != std::errc::no_such_file_or_directory &&
            error != std::errc::not_a_directory)
            return failure(other, "Nao foi possivel verificar aliases de MASMAPRG.ASM", error);

        // weakly_canonical leaves dangling leaf symlinks unresolved. Follow their
        // targets explicitly so aliases are rejected even before publication.
        const auto status = fs::symlink_status(canonical, error);
        if (error && error != std::errc::no_such_file_or_directory &&
            error != std::errc::not_a_directory)
            return failure(other, "Nao foi possivel consultar o caminho protegido", error);
        if (!fs::is_symlink(status))
            return std::nullopt;
        const auto target = fs::read_symlink(canonical, error);
        if (error)
            return failure(other, "Nao foi possivel resolver o link protegido", error);
        absolute = target.is_absolute() ? target : canonical.parent_path() / target;
    }
    return failure(other, "Limite de links ao verificar aliases de MASMAPRG.ASM");
}

} // namespace

ExpandedSourceResult publishExpandedSource(std::string_view text,
                                           const fs::path& source,
                                           const fs::path& binary,
                                           const fs::path& listing) {
    std::error_code error;
    const auto workingDirectory = fs::current_path(error);
    if (error)
        return failure("MASMAPRG.ASM", "Diretorio de trabalho indisponivel", error);
    const auto destination = workingDirectory / "MASMAPRG.ASM";
    const auto status = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory)
        return failure(destination, "Nao foi possivel consultar a fonte expandida", error);
    if (!error && status.type() != fs::file_type::not_found && !fs::is_regular_file(status))
        return failure(destination,
                       "MASMAPRG.ASM existente precisa ser arquivo regular, sem symlink");
    for (const auto* path : {&source, &binary, &listing})
        if (auto diagnostic = protect(destination, *path))
            return *diagnostic;

    StagingDirectory staging;
    static std::atomic<unsigned int> sequence{};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100 && staging.path.empty(); ++attempt) {
        const auto candidate = workingDirectory / (".sicxe-macro-source-" + std::to_string(stamp) +
                                                   "-" + std::to_string(sequence++));
        error.clear();
        if (fs::create_directory(candidate, error))
            staging.path = candidate;
        else if (error && error != std::errc::file_exists)
            return failure(destination, "Nao foi possivel preparar a fonte expandida", error);
    }
    if (staging.path.empty())
        return failure(destination, "Nao foi possivel criar area temporaria exclusiva");
    const auto temporary = staging.path / "source";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    const bool written = file.good();
    file.close();
    if (!written || file.fail())
        return failure(destination, "Falha ao gravar completamente a fonte expandida");
    // Same filesystem: no delete/truncate of the previous destination first.
    fs::rename(temporary, destination, error);
    if (error)
        return failure(destination, "Falha na substituicao atomica da fonte expandida", error);
    return destination;
}

} // namespace macro
