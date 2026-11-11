#pragma once

#include "MacroDiagnostic.hpp"
#include "ParameterSubstitution.hpp"
#include "SourceLine.hpp"
#include "SourceOrigin.hpp"
#include "SourcePosition.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace macro {

using LineProcessResult = std::variant<SourceLine, MacroDiagnostic>;
using ExpandedLinesResult = std::variant<std::vector<SourceLine>, MacroDiagnostic>;

class LocalLabelSerial {
public:
    explicit LocalLabelSerial(std::size_t maxSerial = std::numeric_limits<std::size_t>::max());

    // Aloca o próximo número serial determinístico (1, 2, ...) para uma chamada expandida.
    // Retorna ExpansionLimitExceeded caso o limite representável seja ultrapassado.
    std::variant<std::size_t, MacroDiagnostic> allocateSerial(
        SourcePosition invocationPosition = {1, 1},
        std::span<const ExpansionFrame> calls = {});

    // Processa uma linha comum da fonte (fora de expansão de macro).
    // - Verifica se o rótulo colide com símbolos já definidos (comuns ou gerados).
    // - Registra o rótulo na tabela de símbolos emitidos.
    // - Registra a origem correspondente (SourceOrigin) com expandedLine consecutivo e calls vazia.
    LineProcessResult processCommonLine(const SourceLine& line);

    // Processa uma linha emitida por uma chamada expandida de macro.
    // - Substitui ocorrências de NOME.SER (sem distinção de caixa em .SER) pelo identificador NOME_M<serial>.
    // - Garante que todas as ocorrências de um mesmo prefixo na chamada recebam o mesmo nome gerado.
    // - Valida gramática do identificador gerado e rejeição de nomes reservados.
    // - Detecta colisões do rótulo definido com símbolos já existentes.
    // - Atualiza colunas de byte para o texto renderizado, mantendo número de linha física original.
    // - Registra a origem (SourceOrigin) com expandedLine consecutivo e a cadeia de chamadas informada.
    LineProcessResult processExpandedLine(
        const SourceLine& line,
        std::size_t serial,
        std::span<const ExpansionFrame> calls = {});

    // Processa sequencialmente um conjunto de linhas de uma chamada expandida com o serial informado.
    ExpandedLinesResult processExpandedLines(
        std::span<const SourceLine> lines,
        std::size_t serial,
        std::span<const ExpansionFrame> calls = {});

    // Aloca o próximo serial e processa todas as linhas informadas da chamada.
    ExpandedLinesResult expandCall(
        std::span<const SourceLine> lines,
        SourcePosition invocationPosition,
        std::span<const ExpansionFrame> calls = {});

    // Processa o resultado de ParameterSubstitution::instantiate, ignorando intervalos
    // de definições de macros internas diretas (estrutura que só é emitida quando chamada).
    ExpandedLinesResult processSubstitutionResult(
        const SubstitutionResult& substitution,
        std::size_t serial,
        std::span<const ExpansionFrame> calls = {});

    // Origens coletadas para todas as linhas físicas emitidas até o momento.
    const std::vector<SourceOrigin>& origins() const noexcept;
    std::vector<SourceOrigin> takeOrigins() noexcept;

    // Consultas de estado
    std::size_t currentSerial() const noexcept;
    std::size_t nextExpandedLine() const noexcept;
    std::size_t maxSerial() const noexcept;
    void setMaxSerial(std::size_t limit) noexcept;

    bool isLabelDefined(std::string_view name) const;
    std::optional<SourcePosition> findLabelDefinition(std::string_view name) const;

    // Helpers públicos para verificação e formatação de rótulos locais
    static bool isLocalLabelMarker(std::string_view token);
    static std::string_view extractPrefix(std::string_view token);
    static std::string formatGeneratedName(std::string_view prefix, std::size_t serial);

private:
    struct LabelDefinition {
        SourcePosition position;
        std::string originalSpelling;
    };

    std::size_t serialCounter_{1};
    std::size_t maxSerial_{std::numeric_limits<std::size_t>::max()};
    std::size_t nextExpandedLine_{1};

    // Tabela de rótulos emitidos na fonte expandida: chave normalizada -> definição
    std::unordered_map<std::string, LabelDefinition> definedLabels_;

    // Cache por chamada: serial -> (chave normalizada do prefixo -> identificador gerado)
    std::unordered_map<std::size_t, std::unordered_map<std::string, std::string>> callLabels_;

    // Origem por linha física de saída
    std::vector<SourceOrigin> origins_;

    std::variant<std::string, MacroDiagnostic> resolveGeneratedName(
        std::string_view prefix,
        std::size_t serial,
        SourcePosition tokenPosition,
        std::span<const ExpansionFrame> calls);
};

} // namespace macro
