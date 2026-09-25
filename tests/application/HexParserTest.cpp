#include "application/HexParser.hpp"

#include <gtest/gtest.h>

TEST(HexParserTest, AcceptsWhitespaceAndMixedCaseWithoutChangingAddresses) {
    const auto result = HexParser::parse(" \t00 aB\r\nCd FF\v\f", 32, 34);
    EXPECT_EQ(std::get<ProgramInput>(result), (ProgramInput{{0, 0xAB, 0xCD, 0xFF}, 32, 34}));
}

TEST(HexParserTest, ReportsEmptyInput) {
    for (const auto text : {"", " \t\r\n"})
        EXPECT_EQ(std::get<InputFault>(HexParser::parse(text, 0, 0)),
                  (InputFault{InputFaultCode::EmptyInput}));
}

TEST(HexParserTest, ReportsPreciseInvalidTokenOffset) {
    EXPECT_EQ(std::get<InputFault>(HexParser::parse("00 G1", 0, 0)),
              (InputFault{InputFaultCode::InvalidDigit, 3}));
    EXPECT_EQ(std::get<InputFault>(HexParser::parse("00 A", 0, 0)),
              (InputFault{InputFaultCode::IncompleteByte, 3}));
    for (const auto text : {"100", "000", "AABB"})
        EXPECT_EQ(std::get<InputFault>(HexParser::parse(text, 0, 0)),
                  (InputFault{InputFaultCode::ByteOutOfRange, 0}));
    for (const auto text : {"0x00", "-1", "AA,BB", "LDA", "H^COPY"})
        EXPECT_EQ(std::get<InputFault>(HexParser::parse(text, 0, 0)).code,
                  InputFaultCode::InvalidDigit);
}
