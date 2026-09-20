#pragma once

#include <array>
#include <cstdint>
#include <variant>

enum class Opcode : std::uint8_t {
    ADD = 0x18,
    ADDR = 0x90,
    AND = 0x40,
    CLEAR = 0xB4,
    COMP = 0x28,
    COMPR = 0xA0,
    DIV = 0x24,
    DIVR = 0x9C,
    J = 0x3C,
    JEQ = 0x30,
    JGT = 0x34,
    JLT = 0x38,
    JSUB = 0x48,
    LDA = 0x00,
    LDB = 0x68,
    LDCH = 0x50,
    LDL = 0x08,
    LDS = 0x6C,
    LDT = 0x74,
    LDX = 0x04,
    MUL = 0x20,
    MULR = 0x98,
    OR = 0x44,
    RMO = 0xAC,
    RSUB = 0x4C,
    SHIFTL = 0xA4,
    SHIFTR = 0xA8,
    STA = 0x0C,
    STB = 0x78,
    STCH = 0x54,
    STL = 0x14,
    STS = 0x7C,
    STT = 0x84,
    STX = 0x10,
    SUB = 0x1C,
    SUBR = 0x94,
    TIX = 0x2C,
    TIXR = 0xB8,
};

enum class InstructionFormat : std::uint8_t {
    Sic,
    Format2,
    Format3,
    Format4,
};

struct DecodedInstruction {
    Opcode opcode{};
    InstructionFormat format{};
    std::uint32_t instructionAddress{};
    std::uint8_t length{};
    std::array<std::uint8_t, 4> rawBytes{};

    std::uint8_t r1{};
    std::uint8_t r2{};

    bool n{};
    bool i{};
    bool x{};
    bool b{};
    bool p{};
    bool e{};
    std::uint32_t encodedOperand{};
};

enum class DecodeFaultCode {
    UnknownOpcode,
    TruncatedInstruction,
    InvalidRegister,
    InvalidFlagCombination,
    InvalidOperandEncoding,
};

struct DecodeFault {
    DecodeFaultCode code{};
    std::uint32_t instructionAddress{};
    std::array<std::uint8_t, 4> rawBytes{};
    std::uint8_t bytesRead{};
};

using DecodeResult = std::variant<DecodedInstruction, DecodeFault>;
