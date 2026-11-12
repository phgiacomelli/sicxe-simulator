#pragma once

#include "macros/MacroAssemblerCli.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace macro::test {
namespace fs = std::filesystem;

class MacroAssemblerFiles : public testing::Test {
protected:
    fs::path directory;
    fs::path previousDirectory;
    std::ostringstream output;
    std::ostringstream errors;

    void SetUp() override {
        previousDirectory = fs::current_path();
        static std::atomic<unsigned int> sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate =
                fs::temp_directory_path() /
                ("sicxe-macro-cli-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
            if (fs::create_directory(candidate)) {
                directory = candidate;
                fs::current_path(directory);
                return;
            }
        }
        FAIL() << "Cannot create temporary working directory";
    }

    void TearDown() override {
        std::error_code error;
        fs::current_path(previousDirectory, error);
        EXPECT_FALSE(error) << error.message();
        if (!directory.empty())
            fs::remove_all(directory, error);
    }

    void write(const fs::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary);
        file << text;
        file.close();
        ASSERT_FALSE(file.fail());
    }

    std::string read(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        EXPECT_TRUE(file.good()) << path;
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    int invoke(std::initializer_list<std::string> arguments) {
        std::vector<std::string_view> views;
        for (const auto& argument : arguments)
            views.push_back(argument);
        return runMacroAssemblerCli(views, output, errors);
    }

    void expectNoStagingFiles() {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            const auto name = entry.path().filename().string();
            EXPECT_FALSE(name.starts_with(".sicxe-macro-")) << entry.path();
            EXPECT_FALSE(name.starts_with(".sicxe-output-")) << entry.path();
        }
    }
};

} // namespace macro::test
