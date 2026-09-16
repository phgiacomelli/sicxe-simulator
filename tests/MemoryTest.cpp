#include "core/Memory.hpp"

#include <gtest/gtest.h>

TEST(MemoryTest, InitializesWithCorrectSize) {
    Memory memory(1024);

    EXPECT_EQ(memory.size(), 1024);
}

TEST(MemoryTest, InitializesMemoryWithZero) {
    Memory memory(1024);

    EXPECT_EQ(memory.readByte(0), 0);
    EXPECT_EQ(memory.readByte(500), 0);
    EXPECT_EQ(memory.readByte(1023), 0);
}

TEST(MemoryTest, WritesAndReadsByte) {
    Memory memory(1024);

    memory.writeByte(100, 0xAB);

    EXPECT_EQ(memory.readByte(100), 0xAB);
}

TEST(MemoryTest, WritesAndReadsWord) {
    Memory memory(1024);

    memory.writeWord(100, 0x123456);

    EXPECT_EQ(memory.readWord(100), 0x123456);
}

TEST(MemoryTest, StoresWordAsThreeBytes) {
    Memory memory(1024);

    memory.writeWord(100, 0x123456);

    EXPECT_EQ(memory.readByte(100), 0x12);
    EXPECT_EQ(memory.readByte(101), 0x34);
    EXPECT_EQ(memory.readByte(102), 0x56);
}

TEST(MemoryTest, KeepsOnlyLower24Bits) {
    Memory memory(1024);

    memory.writeWord(100, 0xAB123456);

    EXPECT_EQ(memory.readWord(100), 0x123456);
}

TEST(MemoryTest, ThrowsWhenReadingOutsideMemory) {
    Memory memory(1024);

    EXPECT_THROW(memory.readByte(1024), std::out_of_range);
}

TEST(MemoryTest, ThrowsWhenWritingOutsideMemory) {
    Memory memory(1024);

    EXPECT_THROW(memory.writeByte(1024, 0xFF), std::out_of_range);
}

TEST(MemoryTest, ThrowsWhenWordExceedsMemoryBoundary) {
    Memory memory(1024);

    EXPECT_THROW(memory.readWord(1022), std::out_of_range);
    EXPECT_THROW(memory.writeWord(1022, 0x123456), std::out_of_range);
}