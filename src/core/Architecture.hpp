#pragma once

#include <cstdint>
#include <optional>

enum class RegisterId : std::uint8_t {
    A = 0,
    X = 1,
    L = 2,
    B = 3,
    S = 4,
    T = 5,
    PC = 8,
    SW = 9,
};

constexpr std::optional<RegisterId> registerIdFromCode(std::uint8_t code) {
    switch (code) {
        case 0:
            return RegisterId::A;
        case 1:
            return RegisterId::X;
        case 2:
            return RegisterId::L;
        case 3:
            return RegisterId::B;
        case 4:
            return RegisterId::S;
        case 5:
            return RegisterId::T;
        case 8:
            return RegisterId::PC;
        case 9:
            return RegisterId::SW;
        default:
            return std::nullopt;
    }
}

enum class ConditionCode : std::uint8_t {
    Less,
    Equal,
    Greater,
};

inline constexpr std::uint32_t WordMask = 0x00FFFFFF;
inline constexpr std::uint32_t ConditionCodeMask = 0x000003;
