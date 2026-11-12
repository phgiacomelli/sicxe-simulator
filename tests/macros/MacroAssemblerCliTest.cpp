#include "macros/MacroAssemblerCli.hpp"

#include "MacroAssemblerTestSupport.hpp"

#include <array>
#include <gtest/gtest.h>
#include <sstream>
#include <string_view>

namespace macro {
namespace {
namespace fs = std::filesystem;
using MacroAssemblerCliTest = test::MacroAssemblerFiles;

TEST_F(MacroAssemblerCliTest, HelpExplainsTheFixedSourceAndReplacementPolicy) {
    const std::array<std::string_view, 1> arguments{"--help"};
    EXPECT_EQ(runMacroAssemblerCli(arguments, output, errors), 0);
    EXPECT_NE(output.str().find("SICXEMacroAssembler SOURCE"), std::string::npos);
    EXPECT_NE(output.str().find("MASMAPRG.ASM"), std::string::npos);
    EXPECT_NE(output.str().find("diretorio de trabalho"), std::string::npos);
    EXPECT_NE(output.str().find("substitui"), std::string::npos);
    EXPECT_NE(output.str().find("nao sobrescreve"), std::string::npos);
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(MacroAssemblerCliTest, ExpandsThenReplacesTheFixedSourceAndAssemblesTheOriginalBasename) {
    fs::create_directory("inputs");
    const std::string source = " MCDEFN\r\n"
                               "DEST INCR VALUE\r\n"
                               "DEST LDA VALUE\r\n"
                               " ADD #1\r\n"
                               " STA VALUE\r\n"
                               " MCEND\r\n"
                               " START 0x100\r\n"
                               "ENTRY INCR COUNT\r\n"
                               "DONE J DONE\r\n"
                               "COUNT WORD 5\r\n"
                               " END ENTRY\r\n";
    write("inputs/sum program.asm", source);
    write("MASMAPRG.ASM", "previous expanded program");
    ASSERT_EQ(invoke({"inputs/sum program.asm"}), 0) << errors.str();
    EXPECT_EQ(read("MASMAPRG.ASM"),
              " START 0x100\nENTRY LDA COUNT\n ADD #1\n STA COUNT\n"
              "DONE J DONE\nCOUNT WORD 5\n END ENTRY\n");
    EXPECT_EQ(read("inputs/sum program.bin"),
              std::string("\x03\x20\x09\x19\x00\x01\x0F\x20\x03\x3F\x2F\xFD\x00\x00\x05", 15));
    EXPECT_NE(read("inputs/sum program.lst").find("03 20 09\tENTRY LDA COUNT"), std::string::npos);
    EXPECT_EQ(read("inputs/sum program.asm"), source);
    EXPECT_FALSE(fs::exists("MASMAPRG.bin"));
    EXPECT_FALSE(fs::exists("inputs/MASMAPRG.ASM"));
    EXPECT_NE(output.str().find("0x000100"), std::string::npos);
    EXPECT_TRUE(errors.str().empty());
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, ProtectsTheFixedSourceFromOutputAliasesBeforeFirstPublication) {
    write("demo.asm", " LDA #1\n END\n");
    for (const auto* option : {"-o", "-l"}) {
        EXPECT_EQ(invoke({"demo.asm", option, "MASMAPRG.ASM"}), 1);
        EXPECT_FALSE(fs::exists("MASMAPRG.ASM"));
        EXPECT_FALSE(fs::exists("demo.bin"));
        EXPECT_FALSE(fs::exists("demo.lst"));
    }
    EXPECT_EQ(read("demo.asm"), " LDA #1\n END\n");
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, DanglingOutputAliasesPreventTheFirstPublication) {
    write("demo.asm", " LDA #1\n END\n");
    std::error_code error;
    fs::create_symlink("MASMAPRG.ASM", "alias.out", error);
    if (error)
        GTEST_SKIP() << error.message();
    for (const auto* option : {"-o", "-l"}) {
        EXPECT_EQ(invoke({"demo.asm", option, "alias.out"}), 1);
        EXPECT_FALSE(fs::exists("MASMAPRG.ASM"));
        EXPECT_FALSE(fs::exists("demo.bin"));
        EXPECT_FALSE(fs::exists("demo.lst"));
        EXPECT_EQ(fs::read_symlink("alias.out"), fs::path{"MASMAPRG.ASM"});
    }
    EXPECT_EQ(read("demo.asm"), " LDA #1\n END\n");
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, ChainedDanglingOutputAliasesResolveRelativeTargetsAndParentAliases) {
    write("demo.asm", " LDA #1\n END\n");
    fs::create_directory("links");
    std::error_code error;
    fs::create_directory_symlink("..", "links/root", error);
    if (error)
        GTEST_SKIP() << error.message();
    fs::create_symlink("root/MASMAPRG.ASM", "links/target");
    fs::create_symlink("links/target", "alias.out");
    for (const auto* option : {"-o", "-l"}) {
        EXPECT_EQ(invoke({"demo.asm", option, "alias.out"}), 1);
        EXPECT_FALSE(fs::exists("MASMAPRG.ASM"));
        EXPECT_FALSE(fs::exists("demo.bin"));
        EXPECT_FALSE(fs::exists("demo.lst"));
        EXPECT_EQ(fs::read_symlink("alias.out"), fs::path{"links/target"});
        EXPECT_EQ(fs::read_symlink("links/target"), fs::path{"root/MASMAPRG.ASM"});
    }
    EXPECT_EQ(read("demo.asm"), " LDA #1\n END\n");
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, AssemblyErrorsKeepTheNewSourceAndReportNestedOriginalOrigins) {
    write("nested.asm",
          " MCDEFN\n OUTER\n MCDEFN\n INNER\n LDA MISSING\n MCEND\n INNER\n MCEND\n"
          " START 0x100\n OUTER\n END\n");
    write("MASMAPRG.ASM", "old version");
    EXPECT_EQ(invoke({"nested.asm"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), " START 0x100\n LDA MISSING\n END\n");
    EXPECT_NE(errors.str().find("MASMAPRG.ASM:2:6: error:"), std::string::npos);
    EXPECT_NE(errors.str().find("origem: nested.asm:5:1"), std::string::npos);
    EXPECT_NE(errors.str().find("macro OUTER: definicao nested.asm:2:2; chamada nested.asm:10:2"),
              std::string::npos);
    EXPECT_NE(errors.str().find("macro INNER: definicao nested.asm:4:2; chamada nested.asm:7:2"),
              std::string::npos);
    EXPECT_FALSE(fs::exists("nested.bin"));
    EXPECT_FALSE(fs::exists("nested.lst"));
    EXPECT_TRUE(output.str().empty());
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, CustomPathsPreserveRawBytesAndUseLongOptions) {
    fs::create_directory("bin output");
    fs::create_directory("listing output");
    write("raw.asm", " BYTE X'0080FF0A0D'\n END\n");
    ASSERT_EQ(
        invoke(
            {"raw.asm", "--output", "bin output/raw.data", "--listing", "listing output/raw.text"}),
        0)
        << errors.str();
    EXPECT_EQ(read("bin output/raw.data"), std::string("\x00\x80\xFF\x0A\x0D", 5));
    EXPECT_TRUE(fs::exists("listing output/raw.text"));
    EXPECT_FALSE(fs::exists("raw.bin"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, InvalidArgumentsDoNotTouchThePreviousExpansion) {
    write("MASMAPRG.ASM", "previous");
    for (const auto& arguments :
         std::vector<std::vector<std::string>>{{},
                                               {"--unknown"},
                                               {"a.asm", "b.asm"},
                                               {"demo.asm", "-o"},
                                               {"demo.asm", "-l", ""},
                                               {"demo.asm", "-o", "a.bin", "-o", "b.bin"},
                                               {"demo.asm", "--listing", "a.lst", "-l", "b.lst"},
                                               {"demo.asm", "--expanded", "other.asm"}}) {
        std::vector<std::string_view> views;
        for (const auto& argument : arguments)
            views.push_back(argument);
        EXPECT_EQ(runMacroAssemblerCli(views, output, errors), 2);
        EXPECT_EQ(read("MASMAPRG.ASM"), "previous");
    }
    EXPECT_EQ(invoke({"-h"}), 0);
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, DoubleDashAllowsASourceNameBeginningWithDash) {
    write("-demo.asm", " LDA #1\n END\n");
    ASSERT_EQ(invoke({"--", "-demo.asm"}), 0) << errors.str();
    EXPECT_EQ(read("-demo.bin"), std::string("\x01\x00\x01", 3));
}

TEST_F(MacroAssemblerCliTest, ReadFailuresPreserveThePreviousExpandedFile) {
    write("MASMAPRG.ASM", "previous");
    EXPECT_EQ(invoke({"missing.asm"}), 1);
    fs::create_directory("source-directory");
    EXPECT_EQ(invoke({"source-directory", "-o", "out.bin", "-l", "out.lst"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), "previous");
    EXPECT_FALSE(fs::exists("out.bin"));
    EXPECT_FALSE(fs::exists("out.lst"));
    EXPECT_TRUE(output.str().empty());
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, MacroErrorsPreserveThePreviousFileAndShowTheCallChain) {
    write("cycle.asm", " MCDEFN\n AGAIN\n AGAIN\n MCEND\n AGAIN\n");
    write("MASMAPRG.ASM", "previous");
    EXPECT_EQ(invoke({"cycle.asm"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), "previous");
    EXPECT_NE(errors.str().find("cycle.asm:3:2: error:"), std::string::npos);
    EXPECT_NE(errors.str().find("macro AGAIN: definicao cycle.asm:2:2; chamada cycle.asm:5:2"),
              std::string::npos);
    EXPECT_NE(errors.str().find("chamada cycle.asm:3:2"), std::string::npos);
    EXPECT_FALSE(fs::exists("cycle.bin"));
    EXPECT_FALSE(fs::exists("cycle.lst"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, FirstExpansionFailureDoesNotCreateAnyOutput) {
    write("bad.asm", " MCEND\n");
    EXPECT_EQ(invoke({"bad.asm"}), 1);
    EXPECT_NE(errors.str().find("bad.asm:1:2: error:"), std::string::npos);
    EXPECT_FALSE(fs::exists("MASMAPRG.ASM"));
    EXPECT_FALSE(fs::exists("bad.bin"));
    EXPECT_FALSE(fs::exists("bad.lst"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, ExistingBinaryIsPreservedButSuccessfulExpansionIsRetained) {
    write("demo.asm", " LDA #1\n END\n");
    write("demo.bin", "previous binary");
    write("MASMAPRG.ASM", "previous expansion");
    EXPECT_EQ(invoke({"demo.asm"}), 1);
    EXPECT_EQ(read("demo.bin"), "previous binary");
    EXPECT_EQ(read("MASMAPRG.ASM"), " LDA #1\n END\n");
    EXPECT_FALSE(fs::exists("demo.lst"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, ExistingListingIsPreservedButSuccessfulExpansionIsRetained) {
    write("demo.asm", " LDA #1\n END\n");
    write("demo.lst", "previous listing");
    EXPECT_EQ(invoke({"demo.asm"}), 1);
    EXPECT_EQ(read("demo.lst"), "previous listing");
    EXPECT_EQ(read("MASMAPRG.ASM"), " LDA #1\n END\n");
    EXPECT_FALSE(fs::exists("demo.bin"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, SecondOutputPreparationFailureLeavesNoPartialBinary) {
    write("demo.asm", " LDA #1\n END\n");
    EXPECT_EQ(invoke({"demo.asm", "-l", "missing/out.lst"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), " LDA #1\n END\n");
    EXPECT_FALSE(fs::exists("demo.bin"));
    EXPECT_FALSE(fs::exists("missing"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, OutputAliasesDoNotDestroyTheOriginalSource) {
    const std::string text = " LDA #1\n END\n";
    write("demo.asm", text);
    EXPECT_EQ(invoke({"demo.asm", "-o", "same.out", "-l", "./same.out"}), 1);
    EXPECT_FALSE(fs::exists("same.out"));
    EXPECT_EQ(invoke({"demo.asm", "-o", "demo.asm"}), 1);
    EXPECT_EQ(read("demo.asm"), text);
    EXPECT_EQ(read("MASMAPRG.ASM"), text);
    EXPECT_FALSE(fs::exists("demo.lst"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, TheFixedSourceCannotBeTheInput) {
    const std::string text = " MCDEFN\n DEMO\n LDA #1\n MCEND\n DEMO\n END\n";
    write("MASMAPRG.ASM", text);
    EXPECT_EQ(invoke({"./MASMAPRG.ASM", "-o", "out.bin", "-l", "out.lst"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), text);
    EXPECT_FALSE(fs::exists("out.bin"));
    EXPECT_FALSE(fs::exists("out.lst"));
}

TEST_F(MacroAssemblerCliTest, HardLinkedInputCannotAliasTheFixedSource) {
    const std::string text = " LDA #1\n END\n";
    write("MASMAPRG.ASM", text);
    std::error_code error;
    fs::create_hard_link("MASMAPRG.ASM", "input.asm", error);
    if (error)
        GTEST_SKIP() << error.message();
    EXPECT_EQ(invoke({"input.asm"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), text);
    EXPECT_EQ(read("input.asm"), text);
    EXPECT_FALSE(fs::exists("input.bin"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, HardLinkedOutputCannotAliasThePreviousExpandedFile) {
    write("demo.asm", " LDA #1\n END\n");
    write("MASMAPRG.ASM", "previous");
    std::error_code error;
    fs::create_hard_link("MASMAPRG.ASM", "alias.lst", error);
    if (error)
        GTEST_SKIP() << error.message();
    EXPECT_EQ(invoke({"demo.asm", "-l", "alias.lst"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), "previous");
    EXPECT_EQ(read("alias.lst"), "previous");
    EXPECT_FALSE(fs::exists("demo.bin"));
}

TEST_F(MacroAssemblerCliTest, SymlinkAtFixedDestinationCannotOverwriteTheInput) {
    const std::string text = " LDA #1\n END\n";
    write("demo.asm", text);
    std::error_code error;
    fs::create_symlink("demo.asm", "MASMAPRG.ASM", error);
    if (error)
        GTEST_SKIP() << error.message();
    EXPECT_EQ(invoke({"demo.asm"}), 1);
    EXPECT_TRUE(fs::is_symlink("MASMAPRG.ASM"));
    EXPECT_EQ(read("demo.asm"), text);
    EXPECT_FALSE(fs::exists("demo.bin"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, DanglingFixedSymlinkDoesNotCreateItsTarget) {
    write("demo.asm", " LDA #1\n END\n");
    std::error_code error;
    fs::create_symlink("missing.asm", "MASMAPRG.ASM", error);
    if (error)
        GTEST_SKIP() << error.message();
    EXPECT_EQ(invoke({"demo.asm"}), 1);
    EXPECT_TRUE(fs::is_symlink("MASMAPRG.ASM"));
    EXPECT_FALSE(fs::exists("missing.asm"));
    EXPECT_FALSE(fs::exists("demo.bin"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, DirectoryAtFixedDestinationPreventsAssemblyAndKeepsItsContents) {
    write("demo.asm", " LDA #1\n END\n");
    fs::create_directory("MASMAPRG.ASM");
    write("MASMAPRG.ASM/keep", "previous");
    EXPECT_EQ(invoke({"demo.asm"}), 1);
    EXPECT_TRUE(fs::is_directory("MASMAPRG.ASM"));
    EXPECT_EQ(read("MASMAPRG.ASM/keep"), "previous");
    EXPECT_FALSE(fs::exists("demo.bin"));
    EXPECT_FALSE(fs::exists("demo.lst"));
    expectNoStagingFiles();
}

TEST_F(MacroAssemblerCliTest, UnknownCallBeforeItsDefinitionIsLeftForTheAssembler) {
    write("forward.asm", " FUTURE\n MCDEFN\n FUTURE\n LDA #1\n MCEND\n END\n");
    EXPECT_EQ(invoke({"forward.asm"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), " FUTURE\n END\n");
    EXPECT_NE(errors.str().find("MASMAPRG.ASM:1:2: error:"), std::string::npos);
    EXPECT_NE(errors.str().find("origem: forward.asm:1:1"), std::string::npos);
    EXPECT_FALSE(fs::exists("forward.bin"));
}

TEST_F(MacroAssemblerCliTest, LateEncodingFailureDoesNotPublishEarlierValidInstructions) {
    const std::string text = " LDA #1\n LDA FAR\n RESB 4096\nFAR WORD 0\n END\n";
    write("far.asm", text);
    EXPECT_EQ(invoke({"far.asm"}), 1);
    EXPECT_EQ(read("MASMAPRG.ASM"), text);
    EXPECT_NE(errors.str().find("MASMAPRG.ASM:2:6: error:"), std::string::npos);
    EXPECT_FALSE(fs::exists("far.bin"));
    EXPECT_FALSE(fs::exists("far.lst"));
    expectNoStagingFiles();
}

} // namespace
} // namespace macro
