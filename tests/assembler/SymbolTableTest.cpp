#include "assembler/SymbolTable.hpp"

#include "assembler/SourceParser.hpp"

#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace {
using namespace assembler;

TEST(SymbolTableTest, StartsEmptyWithoutDefinitionsOrPendingReferences) {
    const SymbolTable table;
    EXPECT_TRUE(table.empty());
    EXPECT_EQ(table.size(), 0U);
    EXPECT_TRUE(table.symbols().empty());
    EXPECT_TRUE(table.references().empty());
    EXPECT_TRUE(table.pendingReferences().empty());
    EXPECT_FALSE(table.hasPendingReferences());
    EXPECT_TRUE(table.unresolvedDiagnostics().empty());
}

TEST(SymbolTableTest, DefinesAndResolvesBackwardReferenceWithCaseNormalization) {
    SymbolTable table;
    EXPECT_FALSE(table.define("loop", 0x1000, SourcePosition{1, 1}).has_value());
    EXPECT_FALSE(table.reference("Loop", SourcePosition{2, 9}).has_value());

    EXPECT_TRUE(table.isDefined("LOOP"));
    EXPECT_TRUE(table.isDefined("loop"));
    EXPECT_FALSE(table.hasPendingReferences());

    const auto found = table.find("lOoP");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, (AssemblySymbol{"LOOP", 0x1000, SourcePosition{1, 1}}));

    const auto resolved = table.resolve("LOOP", SourcePosition{2, 9});
    ASSERT_TRUE(std::holds_alternative<std::uint32_t>(resolved));
    EXPECT_EQ(std::get<std::uint32_t>(resolved), 0x1000U);
}

TEST(SymbolTableTest, KeepsForwardReferencesPendingUntilDefinedAndResolvesMultipleUses) {
    SymbolTable table;
    EXPECT_FALSE(table.reference("target", SourcePosition{1, 10}).has_value());
    EXPECT_FALSE(table.reference("TARGET", SourcePosition{2, 10}).has_value());

    EXPECT_FALSE(table.isDefined("TARGET"));
    EXPECT_TRUE(table.hasPendingReferences());
    ASSERT_EQ(table.pendingReferences().size(), 2U);
    EXPECT_EQ(table.pendingReferences()[0],
              (SymbolReference{"TARGET", "target", SourcePosition{1, 10}}));
    EXPECT_EQ(table.pendingReferences()[1],
              (SymbolReference{"TARGET", "TARGET", SourcePosition{2, 10}}));

    const auto beforeDef = table.resolve("target", SourcePosition{1, 10});
    ASSERT_TRUE(std::holds_alternative<SymbolDiagnostic>(beforeDef));
    EXPECT_EQ(std::get<SymbolDiagnostic>(beforeDef).code, SymbolDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(std::get<SymbolDiagnostic>(beforeDef).position, (SourcePosition{1, 10}));

    EXPECT_FALSE(table.define("Target", 0x2048, SourcePosition{5, 1}).has_value());
    EXPECT_FALSE(table.reference("target", SourcePosition{6, 10}).has_value());

    EXPECT_FALSE(table.hasPendingReferences());
    EXPECT_TRUE(table.pendingReferences().empty());
    EXPECT_TRUE(table.unresolvedDiagnostics().empty());
    ASSERT_EQ(table.references().size(), 3U);

    for (const auto& reference : table.references()) {
        const auto resolved = table.resolve(reference.source, reference.position);
        ASSERT_TRUE(std::holds_alternative<std::uint32_t>(resolved));
        EXPECT_EQ(std::get<std::uint32_t>(resolved), 0x2048U);
    }
}

TEST(SymbolTableTest, RejectsDuplicateDefinitionWithBothPositionsAndPreservesOriginal) {
    SymbolTable table;
    EXPECT_FALSE(table.define("Entry", 0x100, SourcePosition{1, 1}).has_value());

    const auto duplicate = table.define("entry", 0x200, SourcePosition{8, 1});
    ASSERT_TRUE(duplicate.has_value());
    EXPECT_EQ(duplicate->code, SymbolDiagnosticCode::DuplicateSymbol);
    EXPECT_EQ(duplicate->symbol, "entry");
    EXPECT_EQ(duplicate->position, (SourcePosition{8, 1}));
    EXPECT_EQ(duplicate->previousDefinition, (SourcePosition{1, 1}));
    EXPECT_FALSE(duplicate->message.empty());

    ASSERT_EQ(table.size(), 1U);
    EXPECT_EQ(table.symbols().front(), (AssemblySymbol{"ENTRY", 0x100, SourcePosition{1, 1}}));

    const auto resolved = table.resolve("ENTRY", SourcePosition{9, 10});
    ASSERT_TRUE(std::holds_alternative<std::uint32_t>(resolved));
    EXPECT_EQ(std::get<std::uint32_t>(resolved), 0x100U);
}

TEST(SymbolTableTest, ReportsUndefinedSymbolWithUsePositionAndNeverInventsExternalFallback) {
    SymbolTable table;
    EXPECT_FALSE(table.reference("missing_1", SourcePosition{3, 10}).has_value());
    EXPECT_FALSE(table.reference("MISSING_2", SourcePosition{4, 12}).has_value());
    EXPECT_FALSE(table.reference("MISSING_1", SourcePosition{7, 8}).has_value());

    const auto directResolve = table.resolve("never_defined", SourcePosition{10, 9});
    ASSERT_TRUE(std::holds_alternative<SymbolDiagnostic>(directResolve));
    const auto& diagnostic = std::get<SymbolDiagnostic>(directResolve);
    EXPECT_EQ(diagnostic.code, SymbolDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(diagnostic.symbol, "never_defined");
    EXPECT_EQ(diagnostic.position, (SourcePosition{10, 9}));
    EXPECT_FALSE(diagnostic.previousDefinition.has_value());

    const auto unresolved = table.unresolvedDiagnostics();
    ASSERT_EQ(unresolved.size(), 3U);
    EXPECT_EQ(unresolved[0].code, SymbolDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(unresolved[0].symbol, "missing_1");
    EXPECT_EQ(unresolved[0].position, (SourcePosition{3, 10}));
    EXPECT_EQ(unresolved[1].code, SymbolDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(unresolved[1].symbol, "MISSING_2");
    EXPECT_EQ(unresolved[1].position, (SourcePosition{4, 12}));
    EXPECT_EQ(unresolved[2].code, SymbolDiagnosticCode::UndefinedSymbol);
    EXPECT_EQ(unresolved[2].symbol, "MISSING_1");
    EXPECT_EQ(unresolved[2].position, (SourcePosition{7, 8}));
}

TEST(SymbolTableTest, RejectsInvalidIdentifierNamesInDefineReferenceAndResolve) {
    SymbolTable table;
    for (const std::string_view badName :
         {"", "1LOOP", "_LOOP", "BAD-NAME", "BAD$NAME", "A+1", "L(A)", "l\xC3\xA7"}) {
        SCOPED_TRACE(badName);
        const auto defError = table.define(badName, 0x100, SourcePosition{2, 1});
        ASSERT_TRUE(defError.has_value());
        EXPECT_EQ(defError->code, SymbolDiagnosticCode::InvalidSymbolName);
        EXPECT_EQ(defError->position, (SourcePosition{2, 1}));

        const auto refError = table.reference(badName, SourcePosition{3, 8});
        ASSERT_TRUE(refError.has_value());
        EXPECT_EQ(refError->code, SymbolDiagnosticCode::InvalidSymbolName);
        EXPECT_EQ(refError->position, (SourcePosition{3, 8}));

        const auto resError = table.resolve(badName, SourcePosition{4, 8});
        ASSERT_TRUE(std::holds_alternative<SymbolDiagnostic>(resError));
        EXPECT_EQ(std::get<SymbolDiagnostic>(resError).code,
                  SymbolDiagnosticCode::InvalidSymbolName);
        EXPECT_EQ(std::get<SymbolDiagnostic>(resError).position, (SourcePosition{4, 8}));
    }
    EXPECT_TRUE(table.empty());
    EXPECT_TRUE(table.references().empty());
}

TEST(SymbolTableTest, RejectsCollisionsWithMnemonicsAndDirectivesCaseInsensitively) {
    SymbolTable table;
    for (const std::string_view reserved : {"LDA",
                                            "lda",
                                            "Addr",
                                            "CLEAR",
                                            "rsub",
                                            "SHIFTL",
                                            "TIXR",
                                            "J",
                                            "START",
                                            "start",
                                            "END",
                                            "BYTE",
                                            "word",
                                            "RESB",
                                            "RESW",
                                            "BASE",
                                            "nobase"}) {
        SCOPED_TRACE(reserved);
        EXPECT_TRUE(SymbolTable::isReservedName(reserved));

        const auto defError = table.define(reserved, 0x100, SourcePosition{1, 1});
        ASSERT_TRUE(defError.has_value());
        EXPECT_EQ(defError->code, SymbolDiagnosticCode::ReservedSymbolName);
        EXPECT_EQ(defError->position, (SourcePosition{1, 1}));

        const auto refError = table.reference(reserved, SourcePosition{2, 9});
        ASSERT_TRUE(refError.has_value());
        EXPECT_EQ(refError->code, SymbolDiagnosticCode::ReservedSymbolName);
        EXPECT_EQ(refError->position, (SourcePosition{2, 9}));

        const auto resError = table.resolve(reserved, SourcePosition{3, 9});
        ASSERT_TRUE(std::holds_alternative<SymbolDiagnostic>(resError));
        EXPECT_EQ(std::get<SymbolDiagnostic>(resError).code,
                  SymbolDiagnosticCode::ReservedSymbolName);
        EXPECT_EQ(std::get<SymbolDiagnostic>(resError).position, (SourcePosition{3, 9}));
    }

    EXPECT_FALSE(SymbolTable::isReservedName("A"));
    EXPECT_FALSE(SymbolTable::isReservedName("B"));
    EXPECT_FALSE(table.define("A", 0x10, SourcePosition{4, 1}).has_value());
    EXPECT_FALSE(table.define("B", 0x13, SourcePosition{5, 1}).has_value());
    EXPECT_EQ(table.size(), 2U);
}

TEST(SymbolTableTest, ExportsDeterministicSymbolListAndConsumesSourceParserIdentifiers) {
    const auto parsed = SourceParser::parse("zeta LDA #1\nalpha STA beta\nbeta RESW 1\n");
    ASSERT_TRUE(parsed.succeeded());
    ASSERT_EQ(parsed.lines.size(), 3U);

    SymbolTable table;
    EXPECT_FALSE(table.define(*parsed.lines[0].statement->label, 0x1000).has_value());
    EXPECT_FALSE(table.define(*parsed.lines[1].statement->label, 0x1003).has_value());
    EXPECT_FALSE(table.define(*parsed.lines[2].statement->label, 0x1006).has_value());

    const std::vector<AssemblySymbol> expected = {
        {"ZETA", 0x1000, SourcePosition{1, 1}},
        {"ALPHA", 0x1003, SourcePosition{2, 1}},
        {"BETA", 0x1006, SourcePosition{3, 1}},
    };
    EXPECT_EQ(table.symbols(), expected);
}

TEST(SymbolTableTest, SuccessiveAssembliesDoNotShareSymbolsOrReferences) {
    SymbolTable first;
    EXPECT_FALSE(first.define("ONLY_FIRST", 0x500, SourcePosition{1, 1}).has_value());
    EXPECT_FALSE(first.reference("PENDING_FIRST", SourcePosition{2, 10}).has_value());

    SymbolTable second;
    EXPECT_TRUE(second.empty());
    EXPECT_FALSE(second.isDefined("ONLY_FIRST"));
    EXPECT_FALSE(second.hasPendingReferences());
    EXPECT_FALSE(second.define("ONLY_FIRST", 0x900, SourcePosition{1, 1}).has_value());

    EXPECT_EQ(std::get<std::uint32_t>(first.resolve("ONLY_FIRST", SourcePosition{3, 1})), 0x500U);
    EXPECT_EQ(std::get<std::uint32_t>(second.resolve("ONLY_FIRST", SourcePosition{3, 1})), 0x900U);
    EXPECT_TRUE(first.hasPendingReferences());
    EXPECT_FALSE(second.hasPendingReferences());
}

} // namespace
