#include "application/RawBinaryReader.hpp"

#include "application/HexParser.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <sstream>

TEST(RawBinaryReaderTest, PreservesEveryByteAndMatchesHex) {
    const auto path = std::filesystem::temp_directory_path() / "sicxe-all-bytes-test.bin";
    std::ostringstream hex;
    {
        std::ofstream file(path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        for (int byte = 0; byte <= 255; ++byte) {
            file.put(static_cast<char>(byte));
            hex << std::hex << std::setfill('0') << std::setw(2) << byte << ' ';
        }
    }
    const auto result = RawBinaryReader::read(path, 16, 20);
    std::filesystem::remove(path);
    ASSERT_TRUE(std::holds_alternative<ProgramInput>(result));
    EXPECT_EQ(std::get<ProgramInput>(result),
              std::get<ProgramInput>(HexParser::parse(hex.str(), 16, 20)));
    ASSERT_EQ(std::get<ProgramInput>(result).bytes.size(), 256U);
    for (int byte = 0; byte <= 255; ++byte)
        EXPECT_EQ(std::get<ProgramInput>(result).bytes[byte], byte);
}

TEST(RawBinaryReaderTest, EmptyFileAndMissingFileAreStructuredFailures) {
    const auto path = std::filesystem::temp_directory_path() / "sicxe-empty-test.bin";
    {
        std::ofstream file(path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
    }
    EXPECT_EQ(std::get<InputFault>(RawBinaryReader::read(path, 0, 0)).code,
              InputFaultCode::EmptyInput);
    std::filesystem::remove(path);
    EXPECT_EQ(std::get<InputFault>(RawBinaryReader::read(path, 0, 0)).code,
              InputFaultCode::OpenFailed);
}

TEST(RawBinaryReaderTest, ReadFailuresNeverReturnPartialProgram) {
    std::istringstream stream("abc");
    stream.setstate(std::ios::badbit);
    EXPECT_EQ(std::get<InputFault>(RawBinaryReader::read(stream, 0, 0)).code,
              InputFaultCode::ReadFailed);
    std::istringstream throwing("abc");
    throwing.exceptions(std::ios::failbit | std::ios::badbit);
    EXPECT_EQ(std::get<ProgramInput>(RawBinaryReader::read(throwing, 0, 0)).bytes.size(), 3U);
}

namespace {
class FailingBuffer : public std::streambuf {
    char byte_ = 'a';
    bool started_ = false;
    int_type underflow() override {
        if (started_)
            throw std::ios_base::failure("simulated read failure");
        started_ = true;
        setg(&byte_, &byte_, &byte_ + 1);
        return traits_type::to_int_type(byte_);
    }
};
} // namespace

TEST(RawBinaryReaderTest, DiscardsBytesReadBeforeIoFailure) {
    for (bool throws : {false, true}) {
        FailingBuffer buffer;
        std::istream stream(&buffer);
        if (throws)
            stream.exceptions(std::ios::badbit);
        EXPECT_EQ(std::get<InputFault>(RawBinaryReader::read(stream, 0, 0)).code,
                  InputFaultCode::ReadFailed);
    }
}
