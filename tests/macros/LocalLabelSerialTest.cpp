#include "macros/LocalLabelSerial.hpp"

#include "macros/DefinitionCapture.hpp"
#include "macros/MacroArguments.hpp"
#include "macros/MacroTemplate.hpp"
#include "macros/ParameterSubstitution.hpp"
#include "macros/SourceScanner.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace macro {
namespace {

SourceLine scanSingleLine(std::string_view text, std::size_t number = 1) {
    auto lines = SourceScanner::scan(text).lines;
    if (lines.empty()) {
        return SourceLine{};
    }
    auto line = std::move(lines.front());
    line.number = number;
    for (auto* field : {&line.label, &line.operation, &line.operand, &line.comment}) {
        if (*field) {
            (*field)->position.line = number;
        }
    }
    return line;
}

std::vector<SourceLine> scanLines(std::string_view source) {
    return SourceScanner::scan(source).lines;
}

std::vector<std::string> lineTexts(const std::vector<SourceLine>& lines) {
    std::vector<std::string> result;
    result.reserve(lines.size());
    for (const auto& line : lines) {
        result.push_back(line.text);
    }
    return result;
}

} // namespace

TEST(LocalLabelSerialTest, SubstitutesLocalLabelInDefinitionAndReferencesAcrossTwoExpansions) {
    LocalLabelSerial context;

    const auto body = scanLines("LOOP.SER LDA #1\n"
                                "         JEQ LOOP.SER\n");

    const SourcePosition invocation1{10, 2};
    const SourcePosition invocation2{20, 2};
    const std::vector<ExpansionFrame> calls1 = {{"TEST", {1, 2}, invocation1}};
    const std::vector<ExpansionFrame> calls2 = {{"TEST", {1, 2}, invocation2}};

    const auto result1 = context.expandCall(body, invocation1, calls1);
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result1));
    const auto& lines1 = std::get<std::vector<SourceLine>>(result1);

    EXPECT_EQ(lineTexts(lines1),
              (std::vector<std::string>{
                  "LOOP_M1 LDA #1",
                  "         JEQ LOOP_M1",
              }));

    const auto result2 = context.expandCall(body, invocation2, calls2);
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result2));
    const auto& lines2 = std::get<std::vector<SourceLine>>(result2);

    EXPECT_EQ(lineTexts(lines2),
              (std::vector<std::string>{
                  "LOOP_M2 LDA #1",
                  "         JEQ LOOP_M2",
              }));

    // Dentro de cada chamada os nomes coincidem; entre chamadas diferem.
    EXPECT_EQ(lines1[0].label->text, "LOOP_M1");
    EXPECT_NE(lines1[0].label->text, lines2[0].label->text);
    EXPECT_EQ(lines2[0].label->text, "LOOP_M2");
}

TEST(LocalLabelSerialTest, NestedCallConsumesDistinctSerialWithoutResettingCounter) {
    LocalLabelSerial context;

    // Chamada externa consome serial 1
    const auto outerSerialResult = context.allocateSerial({10, 1});
    ASSERT_TRUE(std::holds_alternative<std::size_t>(outerSerialResult));
    const std::size_t outerSerial = std::get<std::size_t>(outerSerialResult);
    EXPECT_EQ(outerSerial, 1u);

    const std::vector<ExpansionFrame> outerCall = {{"OUTER", {1, 2}, {10, 1}}};

    // Linha 1 da externa usa serial 1
    const auto line1 = scanSingleLine("LOOP.SER LDA #1\n", 2);
    const auto res1 = context.processExpandedLine(line1, outerSerial, outerCall);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(res1));
    EXPECT_EQ(std::get<SourceLine>(res1).text, "LOOP_M1 LDA #1");

    // Chamada aninhada consome serial distinto (serial 2)
    const auto innerSerialResult = context.allocateSerial({3, 10}, outerCall);
    ASSERT_TRUE(std::holds_alternative<std::size_t>(innerSerialResult));
    const std::size_t innerSerial = std::get<std::size_t>(innerSerialResult);
    EXPECT_EQ(innerSerial, 2u);

    std::vector<ExpansionFrame> nestedCalls = outerCall;
    nestedCalls.push_back({"INNER", {20, 2}, {3, 10}});

    const auto innerLine = scanSingleLine("LOOP.SER LDA #2\n", 21);
    const auto resInner = context.processExpandedLine(innerLine, innerSerial, nestedCalls);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(resInner));
    EXPECT_EQ(std::get<SourceLine>(resInner).text, "LOOP_M2 LDA #2");

    // Retorno à chamada externa continua usando serial 1 sem reiniciar contador
    const auto line2 = scanSingleLine("         JEQ LOOP.SER\n", 4);
    const auto res2 = context.processExpandedLine(line2, outerSerial, outerCall);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(res2));
    EXPECT_EQ(std::get<SourceLine>(res2).text, "         JEQ LOOP_M1");

    // Próxima chamada externa recebe serial 3
    const auto nextCallSerial = context.allocateSerial({30, 1});
    ASSERT_TRUE(std::holds_alternative<std::size_t>(nextCallSerial));
    EXPECT_EQ(std::get<std::size_t>(nextCallSerial), 3u);
}

TEST(LocalLabelSerialTest, CaseInsensitiveMarkerMatchingAndCoincidenceWithinExpansion) {
    EXPECT_TRUE(LocalLabelSerial::isLocalLabelMarker("LOOP.SER"));
    EXPECT_TRUE(LocalLabelSerial::isLocalLabelMarker("loop.ser"));
    EXPECT_TRUE(LocalLabelSerial::isLocalLabelMarker("Loop.Ser"));
    EXPECT_TRUE(LocalLabelSerial::isLocalLabelMarker("x.sEr"));
    EXPECT_FALSE(LocalLabelSerial::isLocalLabelMarker("LOOP.SERVICE"));
    EXPECT_FALSE(LocalLabelSerial::isLocalLabelMarker("LOOP.SER1"));
    EXPECT_FALSE(LocalLabelSerial::isLocalLabelMarker("SER"));

    EXPECT_EQ(LocalLabelSerial::extractPrefix("LOOP.SER"), "LOOP");
    EXPECT_EQ(LocalLabelSerial::extractPrefix("loop.ser"), "loop");
    EXPECT_EQ(LocalLabelSerial::extractPrefix(".SER"), "");

    LocalLabelSerial context;
    const auto body = scanLines("loop.ser LDA #1\n"
                                "         JEQ LOOP.SER\n"
                                "         JLT Loop.Ser\n");

    const auto result = context.expandCall(body, {5, 1});
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result));
    const auto& lines = std::get<std::vector<SourceLine>>(result);

    // Todas as variações de caixa do mesmo prefixo geram exatamente o mesmo identificador
    EXPECT_EQ(lineTexts(lines),
              (std::vector<std::string>{
                  "loop_M1 LDA #1",
                  "         JEQ loop_M1",
                  "         JLT loop_M1",
              }));
}

TEST(LocalLabelSerialTest, PreservesMarkersInsideCommentsAndQuotedLiterals) {
    LocalLabelSerial context;
    const auto body = scanLines(". LOOP.SER full line comment\n"
                                "LOOP.SER LDA #1 ; inline LOOP.SER comment\n"
                                "         BYTE C'LOOP.SER'\n"
                                "         BYTE X'05'\n"
                                "         TITLE 'PREFIX.SER'\n");

    const auto result = context.expandCall(body, {1, 1});
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result));
    const auto& lines = std::get<std::vector<SourceLine>>(result);

    EXPECT_EQ(lineTexts(lines),
              (std::vector<std::string>{
                  ". LOOP.SER full line comment",
                  "LOOP_M1 LDA #1 ; inline LOOP.SER comment",
                  "         BYTE C'LOOP.SER'",
                  "         BYTE X'05'",
                  "         TITLE 'PREFIX.SER'",
              }));
}

TEST(LocalLabelSerialTest, DiagnosesInvalidPrefixMarkerEmptyPrefix) {
    LocalLabelSerial context;
    const auto line = scanSingleLine("         JEQ .SER\n", 10);
    const auto result = context.processExpandedLine(line, 1);

    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(result));
    const auto& diag = std::get<MacroDiagnostic>(result);
    EXPECT_EQ(diag.code, MacroDiagnosticCode::InvalidLabel);
    EXPECT_EQ(diag.position, (SourcePosition{10, 14}));
}

TEST(LocalLabelSerialTest, DiagnosesInvalidPrefixMarkerStartingWithDigitOrUnderscore) {
    LocalLabelSerial context;

    const auto lineDigit = scanSingleLine("         JEQ 1BAD.SER\n", 12);
    const auto resultDigit = context.processExpandedLine(lineDigit, 1);
    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(resultDigit));
    const auto& diagDigit = std::get<MacroDiagnostic>(resultDigit);
    EXPECT_EQ(diagDigit.code, MacroDiagnosticCode::InvalidLabel);
    EXPECT_EQ(diagDigit.position, (SourcePosition{12, 14}));

    const auto lineUnder = scanSingleLine("_BAD.SER LDA #1\n", 14);
    const auto resultUnder = context.processExpandedLine(lineUnder, 1);
    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(resultUnder));
    const auto& diagUnder = std::get<MacroDiagnostic>(resultUnder);
    EXPECT_EQ(diagUnder.code, MacroDiagnosticCode::InvalidLabel);
    EXPECT_EQ(diagUnder.position, (SourcePosition{14, 1}));
}

TEST(LocalLabelSerialTest, DiagnosesInvalidPrefixMarkerWithMalformedCharacters) {
    LocalLabelSerial context;

    const auto lineDot = scanSingleLine("A.B.SER  LDA #1\n", 15);
    const auto resultDot = context.processExpandedLine(lineDot, 1);
    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(resultDot));
    EXPECT_EQ(std::get<MacroDiagnostic>(resultDot).code, MacroDiagnosticCode::InvalidLabel);
}

TEST(LocalLabelSerialTest, DiagnosesSerialOverflowWhenRepresentationLimitIsExceeded) {
    LocalLabelSerial context;
    context.setMaxSerial(1);

    const auto body = scanLines("LOOP.SER LDA #1\n");

    const auto call1 = context.expandCall(body, {10, 2});
    EXPECT_TRUE(std::holds_alternative<std::vector<SourceLine>>(call1));

    // A segunda chamada esgotaria o limite de serial representável
    const auto call2 = context.expandCall(body, {20, 2});
    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(call2));
    const auto& diag = std::get<MacroDiagnostic>(call2);
    EXPECT_EQ(diag.code, MacroDiagnosticCode::ExpansionLimitExceeded);
    EXPECT_EQ(diag.position, (SourcePosition{20, 2}));
}

TEST(LocalLabelSerialTest, DiagnosesCollisionBetweenGeneratedLabelAndPreexistingCommonLabel) {
    LocalLabelSerial context;

    // Rótulo comum definido antes na fonte
    const auto commonLine = scanSingleLine("LOOP_M1  LDA #0\n", 5);
    const auto commonRes = context.processCommonLine(commonLine);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(commonRes));

    // Macro gera LOOP_M1 com serial 1
    const auto macroLine = scanSingleLine("LOOP.SER LDA #1\n", 25);
    const auto macroRes = context.processExpandedLine(macroLine, 1);

    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(macroRes));
    const auto& diag = std::get<MacroDiagnostic>(macroRes);
    EXPECT_EQ(diag.code, MacroDiagnosticCode::DuplicateLabel);
    EXPECT_EQ(diag.position, (SourcePosition{25, 1}));
    EXPECT_NE(diag.message.find("linha 5"), std::string::npos);
}

TEST(LocalLabelSerialTest, DiagnosesCollisionBetweenCommonLabelAndPreexistingGeneratedLabel) {
    LocalLabelSerial context;

    // Macro gera LOOP_M1 com serial 1
    const auto macroLine = scanSingleLine("LOOP.SER LDA #1\n", 8);
    const auto macroRes = context.processExpandedLine(macroLine, 1);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(macroRes));

    // Rótulo comum que aparece depois na fonte e colide com LOOP_M1
    const auto commonLine = scanSingleLine("LOOP_M1  STA #0\n", 40);
    const auto commonRes = context.processCommonLine(commonLine);

    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(commonRes));
    const auto& diag = std::get<MacroDiagnostic>(commonRes);
    EXPECT_EQ(diag.code, MacroDiagnosticCode::DuplicateLabel);
    EXPECT_EQ(diag.position, (SourcePosition{40, 1}));
    EXPECT_NE(diag.message.find("linha 8"), std::string::npos);
}

TEST(LocalLabelSerialTest, DiagnosesCaseInsensitiveLabelCollision) {
    LocalLabelSerial context;

    const auto commonLine = scanSingleLine("loop_m1  LDA #0\n", 3);
    const auto commonRes = context.processCommonLine(commonLine);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(commonRes));

    const auto macroLine = scanSingleLine("LOOP.SER LDA #1\n", 15);
    const auto macroRes = context.processExpandedLine(macroLine, 1);

    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(macroRes));
    EXPECT_EQ(std::get<MacroDiagnostic>(macroRes).code, MacroDiagnosticCode::DuplicateLabel);
}

TEST(LocalLabelSerialTest, DiagnosesDuplicateLabelGeneratedWithinSameCall) {
    LocalLabelSerial context;

    const auto line1 = scanSingleLine("LOOP.SER LDA #0\n", 5);
    const auto line2 = scanSingleLine("LOOP.SER STA #1\n", 6);

    const auto res1 = context.processExpandedLine(line1, 1);
    EXPECT_TRUE(std::holds_alternative<SourceLine>(res1));

    const auto res2 = context.processExpandedLine(line2, 1);
    ASSERT_TRUE(std::holds_alternative<MacroDiagnostic>(res2));
    const auto& diag = std::get<MacroDiagnostic>(res2);
    EXPECT_EQ(diag.code, MacroDiagnosticCode::DuplicateLabel);
    EXPECT_EQ(diag.position, (SourcePosition{6, 1}));
}

TEST(LocalLabelSerialTest, CommonLabelsRemainIntactAndAreNotAutomaticallyRenamed) {
    LocalLabelSerial context;

    const auto common = scanSingleLine("GLOBAL   LDA #0\n", 1);
    const auto commonRes = context.processCommonLine(common);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(commonRes));
    EXPECT_EQ(std::get<SourceLine>(commonRes).text, "GLOBAL   LDA #0");

    const auto macroOrdinary = scanSingleLine("INTERNAL LDA #1\n", 10);
    const auto macroRes = context.processExpandedLine(macroOrdinary, 1);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(macroRes));
    EXPECT_EQ(std::get<SourceLine>(macroRes).text, "INTERNAL LDA #1");
}

TEST(LocalLabelSerialTest, TracksOriginsForCommonLinesDirectMacroLinesAndNestedCalls) {
    LocalLabelSerial context;

    // 1. Linha comum
    const auto commonLine = scanSingleLine("FIRST    LDA #0\n", 1);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(context.processCommonLine(commonLine)));

    // 2. Linha de macro direta
    const ExpansionFrame frameOuter{"OUTER", {10, 2}, {2, 1}};
    const std::vector<ExpansionFrame> directCalls = {frameOuter};
    const auto directLine = scanSingleLine("LOOP.SER LDA #1\n", 12);
    ASSERT_TRUE(
        std::holds_alternative<SourceLine>(context.processExpandedLine(directLine, 1, directCalls)));

    // 3. Linha de chamada aninhada
    const ExpansionFrame frameInner{"INNER", {30, 2}, {13, 10}};
    const std::vector<ExpansionFrame> nestedCalls = {frameOuter, frameInner};
    const auto nestedLine = scanSingleLine("STEP.SER LDA #2\n", 32);
    ASSERT_TRUE(
        std::holds_alternative<SourceLine>(context.processExpandedLine(nestedLine, 2, nestedCalls)));

    const auto& origins = context.origins();
    ASSERT_EQ(origins.size(), 3u);

    // Linha 1: comum
    EXPECT_EQ(origins[0].expandedLine, 1u);
    EXPECT_EQ(origins[0].source, (SourcePosition{1, 1}));
    EXPECT_TRUE(origins[0].calls.empty());

    // Linha 2: direta
    EXPECT_EQ(origins[1].expandedLine, 2u);
    EXPECT_EQ(origins[1].source, (SourcePosition{12, 1}));
    EXPECT_EQ(origins[1].calls, directCalls);

    // Linha 3: aninhada
    EXPECT_EQ(origins[2].expandedLine, 3u);
    EXPECT_EQ(origins[2].source, (SourcePosition{32, 1}));
    EXPECT_EQ(origins[2].calls, nestedCalls);
}

TEST(LocalLabelSerialTest, OriginAndDataRemainValidAfterDestroyingSourceText) {
    LocalLabelSerial context;
    {
        std::string tempSource = "TEMP.SER LDA #9\n";
        const auto line = scanSingleLine(tempSource, 50);
        const std::vector<ExpansionFrame> calls = {{"DYNAMIC", {1, 1}, {2, 2}}};
        EXPECT_TRUE(std::holds_alternative<SourceLine>(context.processExpandedLine(line, 1, calls)));
        tempSource.clear();
        tempSource = "COMPLETELY OVERWRITTEN WITH JUNK DATA";
    }

    const auto origins = context.takeOrigins();
    ASSERT_EQ(origins.size(), 1u);
    EXPECT_EQ(origins[0].expandedLine, 1u);
    EXPECT_EQ(origins[0].source, (SourcePosition{50, 1}));
    ASSERT_EQ(origins[0].calls.size(), 1u);
    EXPECT_EQ(origins[0].calls[0].macroName, "DYNAMIC");
}

TEST(LocalLabelSerialTest, FieldColumnsAreUpdatedToMatchRenderedTextWhilePhysicalLineNumberIsPreserved) {
    LocalLabelSerial context;
    const auto line = scanSingleLine("LOOP.SER LDA #LOOP.SER\n", 77);
    const auto result = context.processExpandedLine(line, 1);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(result));
    const auto& rendered = std::get<SourceLine>(result);

    EXPECT_EQ(rendered.number, 77u);
    EXPECT_EQ(rendered.text, "LOOP_M1 LDA #LOOP_M1");

    ASSERT_TRUE(rendered.label.has_value());
    EXPECT_EQ(rendered.label->text, "LOOP_M1");
    EXPECT_EQ(rendered.label->position, (SourcePosition{77, 1}));

    ASSERT_TRUE(rendered.operation.has_value());
    EXPECT_EQ(rendered.operation->text, "LDA");
    EXPECT_EQ(rendered.operation->position, (SourcePosition{77, 9}));

    ASSERT_TRUE(rendered.operand.has_value());
    EXPECT_EQ(rendered.operand->text, "#LOOP_M1");
    EXPECT_EQ(rendered.operand->position, (SourcePosition{77, 13}));
}

TEST(LocalLabelSerialTest, MultipleMarkersOnSameLineInLabelAndOperand) {
    LocalLabelSerial context;
    const auto line = scanSingleLine("LOOP.SER LDA TARGET.SER\n", 10);
    const auto result = context.processExpandedLine(line, 5);
    ASSERT_TRUE(std::holds_alternative<SourceLine>(result));
    const auto& rendered = std::get<SourceLine>(result);

    EXPECT_EQ(rendered.text, "LOOP_M5 LDA TARGET_M5");
    EXPECT_EQ(rendered.label->text, "LOOP_M5");
    EXPECT_EQ(rendered.operand->text, "TARGET_M5");
}

TEST(LocalLabelSerialTest, ForwardReferenceToLocalLabelWithinMacro) {
    LocalLabelSerial context;
    const auto body = scanLines("         JEQ LOOP.SER\n"
                                "LOOP.SER LDA #0\n");

    const auto result = context.expandCall(body, {1, 1});
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result));
    const auto& lines = std::get<std::vector<SourceLine>>(result);

    EXPECT_EQ(lineTexts(lines),
              (std::vector<std::string>{
                  "         JEQ LOOP_M1",
                  "LOOP_M1 LDA #0",
              }));
}

TEST(LocalLabelSerialTest, AddressingModesPreservedWithLocalLabels) {
    LocalLabelSerial context;
    const auto body = scanLines("         LDA #VAL.SER\n"
                                "         ADD @PTR.SER\n"
                                "         LDX BUF.SER,X\n");

    const auto result = context.expandCall(body, {1, 1});
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result));
    const auto& lines = std::get<std::vector<SourceLine>>(result);

    EXPECT_EQ(lineTexts(lines),
              (std::vector<std::string>{
                  "         LDA #VAL_M1",
                  "         ADD @PTR_M1",
                  "         LDX BUF_M1,X",
              }));
}

TEST(LocalLabelSerialTest, IntegrationWithParameterSubstitutionInstantiate) {
    // Definir e capturar macro com parâmetro e rótulo local
    const std::string defSource = " MCDEFN\n"
                                  " Sample VAL\n"
                                  "LOOP.SER LDA VAL\n"
                                  "         ADD #1\n"
                                  "         STA VAL\n"
                                  "         JEQ LOOP.SER\n"
                                  " MCEND\n";

    const auto scannedDef = SourceScanner::scan(defSource);
    const auto captureResult = DefinitionCapture::capture(scannedDef.lines, 0);
    ASSERT_TRUE(std::holds_alternative<CapturedDefinition>(captureResult));
    const auto& definition = std::get<CapturedDefinition>(captureResult).definition;

    const auto protoResult = MacroArguments::parsePrototype(definition);
    ASSERT_TRUE(std::holds_alternative<MacroPrototype>(protoResult));
    const auto& prototype = std::get<MacroPrototype>(protoResult);

    const auto callLine = scanSingleLine(" Sample COUNT\n");
    const auto bindResult = MacroArguments::bind(prototype, callLine);
    ASSERT_TRUE(std::holds_alternative<BoundMacroArguments>(bindResult));
    const auto& arguments = std::get<BoundMacroArguments>(bindResult);

    const auto templateResult = MacroTemplate::fromDefinition(definition);
    ASSERT_TRUE(std::holds_alternative<MacroTemplate>(templateResult.value));
    const auto& macroTemplate = std::get<MacroTemplate>(templateResult.value);

    // Instanciação substitui o parâmetro VAL por COUNT
    const auto substituted = ParameterSubstitution::instantiate(macroTemplate, arguments);

    // LocalLabelSerial substitui LOOP.SER por LOOP_M1
    LocalLabelSerial context;
    const auto expanded = context.processSubstitutionResult(substituted, 1);
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(expanded));
    const auto& finalLines = std::get<std::vector<SourceLine>>(expanded);

    EXPECT_EQ(lineTexts(finalLines),
              (std::vector<std::string>{
                  "LOOP_M1 LDA COUNT",
                  "         ADD #1",
                  "         STA COUNT",
                  "         JEQ LOOP_M1",
              }));
}

TEST(LocalLabelSerialTest, SkipsInternalMacroDefinitionIntervalsInSubstitutionResult) {
    const std::string source = " MCDEFN\n"
                               " Outer\n"
                               "LOOP.SER LDA #1\n"
                               " MCDEFN\n"
                               " Inner\n"
                               " LDA #2\n"
                               " MCEND\n"
                               "         JEQ LOOP.SER\n"
                               " MCEND\n";

    const auto scanned = SourceScanner::scan(source);
    const auto capture = DefinitionCapture::capture(scanned.lines, 0);
    ASSERT_TRUE(std::holds_alternative<CapturedDefinition>(capture));
    const auto& def = std::get<CapturedDefinition>(capture).definition;

    const auto tmplResult = MacroTemplate::fromDefinition(def);
    ASSERT_TRUE(std::holds_alternative<MacroTemplate>(tmplResult.value));
    const auto& tmpl = std::get<MacroTemplate>(tmplResult.value);

    const auto callLine = scanSingleLine(" Outer\n");
    const auto bind = MacroArguments::bind(tmpl.root().prototype, callLine);
    ASSERT_TRUE(std::holds_alternative<BoundMacroArguments>(bind));
    const auto& args = std::get<BoundMacroArguments>(bind);

    const auto sub = ParameterSubstitution::instantiate(tmpl, args);
    ASSERT_EQ(sub.definitions.size(), 1u);

    LocalLabelSerial context;
    const auto result = context.processSubstitutionResult(sub, 1);
    ASSERT_TRUE(std::holds_alternative<std::vector<SourceLine>>(result));
    const auto& lines = std::get<std::vector<SourceLine>>(result);

    // As linhas estruturais da definicao interna em [1, 5) foram ignoradas
    EXPECT_EQ(lineTexts(lines),
              (std::vector<std::string>{
                  "LOOP_M1 LDA #1",
                  "         JEQ LOOP_M1",
              }));
}

} // namespace macro
