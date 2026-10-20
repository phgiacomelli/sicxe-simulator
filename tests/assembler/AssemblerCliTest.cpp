#include "assembler/AssemblerCli.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace assembler {
namespace {
namespace fs = std::filesystem;

class AssemblerCliTest : public testing::Test {
protected:
    fs::path directory;
    std::ostringstream output;
    std::ostringstream errors;

    void SetUp() override {
        static std::atomic<unsigned int> sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate =
                fs::temp_directory_path() /
                ("sicxe-cli-test-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
            if (fs::create_directory(candidate)) {
                directory = candidate;
                return;
            }
        }
        FAIL() << "Cannot create temporary test directory";
    }

    void TearDown() override {
        if (!directory.empty()) {
            std::error_code error;
            fs::remove_all(directory, error);
        }
    }

    void write(const fs::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary);
        file << text;
        ASSERT_TRUE(file.good());
    }

    std::string read(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    int invoke(std::initializer_list<std::string> arguments) {
        std::vector<std::string_view> views;
        for (const auto& argument : arguments)
            views.push_back(argument);
        return runAssemblerCli(views, output, errors);
    }
};

TEST_F(AssemblerCliTest, WritesBinaryAndListingAndReportsLoadAndEntryAddresses) {
    const auto source = directory / "sum program.asm";
    write(source, " START 0x100\nENTRY LDA #5\n ADD #7\n END ENTRY\n");
    ASSERT_EQ(invoke({source.string()}), 0) << errors.str();
    EXPECT_EQ(read(directory / "sum program.bin"), std::string("\x01\x00\x05\x19\x00\x07", 6));
    EXPECT_NE(read(directory / "sum program.lst").find("01 00 05\tENTRY LDA #5"),
              std::string::npos);
    EXPECT_NE(output.str().find("0x000100"), std::string::npos);
    EXPECT_TRUE(errors.str().empty());
    EXPECT_EQ(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}), 3);
}

TEST_F(AssemblerCliTest, RefusesDanglingSymlinkWithoutCreatingItsTargetOrOtherOutput) {
    const auto source = directory / "demo.asm";
    write(source, " LDA #1\n END\n");
    std::error_code error;
    fs::create_symlink(directory / "missing-target", directory / "demo.bin", error);
    if (error)
        GTEST_SKIP() << "Symlink creation not available: " << error.message();
    EXPECT_EQ(invoke({source.string()}), 1);
    EXPECT_FALSE(fs::exists(directory / "missing-target"));
    EXPECT_FALSE(fs::exists(directory / "demo.lst"));
    EXPECT_TRUE(fs::is_symlink(directory / "demo.bin"));
}

TEST_F(AssemblerCliTest, AllowsDistinctOutputDirectoriesAndPreservesEveryBinaryByte) {
    const auto source = directory / "raw.asm";
    fs::create_directory(directory / "bin");
    fs::create_directory(directory / "listing");
    write(source, " BYTE X'0080FF0A0D'\n END\n");
    const auto binary = directory / "bin" / "custom.data";
    const auto listing = directory / "listing" / "custom.txt";
    ASSERT_EQ(invoke({source.string(), "--output", binary.string(), "--listing", listing.string()}),
              0)
        << errors.str();
    EXPECT_EQ(read(binary), std::string("\x00\x80\xFF\x0A\x0D", 5));
    EXPECT_TRUE(fs::exists(listing));
}

TEST_F(AssemblerCliTest, HelpAndMalformedArgumentsHaveDocumentedExitCodes) {
    EXPECT_EQ(invoke({"--help"}), 0);
    EXPECT_NE(output.str().find("SICXEAssembler"), std::string::npos);
    for (const auto& arguments :
         std::vector<std::vector<std::string>>{{},
                                               {"--unknown"},
                                               {"source.asm", "-o"},
                                               {"a.asm", "b.asm"},
                                               {"a.asm", "-o", "a.bin", "-o", "b.bin"},
                                               {"a.asm", "-l", ""}}) {
        std::vector<std::string_view> views;
        for (const auto& argument : arguments)
            views.push_back(argument);
        EXPECT_EQ(runAssemblerCli(views, output, errors), 2);
    }
}

TEST_F(AssemblerCliTest, SourceOpenAndReadFailuresNeverCreateOutputs) {
    EXPECT_EQ(invoke({(directory / "missing.asm").string()}), 1);
    EXPECT_EQ(invoke({directory.string(),
                      "-o",
                      (directory / "out.bin").string(),
                      "-l",
                      (directory / "out.lst").string()}),
              1);
    EXPECT_FALSE(fs::exists(directory / "out.bin"));
    EXPECT_FALSE(fs::exists(directory / "out.lst"));
    EXPECT_TRUE(output.str().empty());
}

TEST_F(AssemblerCliTest, SyntaxFailureReportsFileLineColumnWithoutPublishingAnything) {
    const auto source = directory / "bad.asm";
    write(source, "BROKEN\n");
    EXPECT_EQ(invoke({source.string()}), 1);
    EXPECT_NE(errors.str().find(source.string() + ":1:7: error:"), std::string::npos);
    EXPECT_FALSE(fs::exists(directory / "bad.bin"));
    EXPECT_FALSE(fs::exists(directory / "bad.lst"));
    EXPECT_TRUE(output.str().empty());
}

TEST_F(AssemblerCliTest, LateEncodingFailureNeverPublishesAnEarlierValidInstruction) {
    const auto source = directory / "far.asm";
    write(source, " LDA #1\n LDA FAR\n RESB 4096\nFAR WORD 0\n END\n");
    EXPECT_EQ(invoke({source.string()}), 1);
    EXPECT_NE(errors.str().find(":2:6: error:"), std::string::npos);
    EXPECT_FALSE(fs::exists(directory / "far.bin"));
    EXPECT_FALSE(fs::exists(directory / "far.lst"));
}

TEST_F(AssemblerCliTest, FailurePreparingSecondOutputCleansStagingAndPublishesNeitherFile) {
    const auto source = directory / "demo.asm";
    write(source, " LDA #1\n END\n");
    EXPECT_EQ(invoke({source.string(), "-l", (directory / "missing" / "out.lst").string()}), 1);
    EXPECT_FALSE(fs::exists(directory / "demo.bin"));
    EXPECT_EQ(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}), 1);
    EXPECT_TRUE(output.str().empty());
}

TEST_F(AssemblerCliTest, ExistingListingIsPreservedAndBinaryIsNotPublished) {
    const auto source = directory / "demo.asm";
    write(source, " LDA #1\n END\n");
    write(directory / "demo.lst", "previous listing");
    EXPECT_EQ(invoke({source.string()}), 1);
    EXPECT_EQ(read(directory / "demo.lst"), "previous listing");
    EXPECT_FALSE(fs::exists(directory / "demo.bin"));
    EXPECT_EQ(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}), 2);
}

TEST_F(AssemblerCliTest, ExistingBinaryIsPreservedAndListingIsNotPublished) {
    const auto source = directory / "demo.asm";
    write(source, " LDA #1\n END\n");
    write(directory / "demo.bin", "previous binary");
    EXPECT_EQ(invoke({source.string()}), 1);
    EXPECT_EQ(read(directory / "demo.bin"), "previous binary");
    EXPECT_FALSE(fs::exists(directory / "demo.lst"));
}

TEST_F(AssemblerCliTest, OutputPathsCannotAliasOneAnotherOrTheInput) {
    const auto source = directory / "demo.asm";
    const std::string text = " LDA #1\n END\n";
    write(source, text);
    EXPECT_EQ(invoke({source.string(),
                      "-o",
                      (directory / "out").string(),
                      "-l",
                      (directory / "." / "out").string()}),
              1);
    EXPECT_FALSE(fs::exists(directory / "out"));
    EXPECT_EQ(invoke({source.string(), "-o", source.string()}), 1);
    EXPECT_EQ(read(source), text);
    EXPECT_FALSE(fs::exists(directory / "demo.lst"));
}

} // namespace
} // namespace assembler
