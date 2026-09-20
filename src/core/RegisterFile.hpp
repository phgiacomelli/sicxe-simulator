#pragma once

#include "Architecture.hpp"

#include <cstdint>

class RegisterFile {
public:
    RegisterFile();

    std::uint32_t getA() const;
    void setA(std::uint32_t value);

    std::uint32_t getX() const;
    void setX(std::uint32_t value);

    std::uint32_t getL() const;
    void setL(std::uint32_t value);

    std::uint32_t getB() const;
    void setB(std::uint32_t value);

    std::uint32_t getS() const;
    void setS(std::uint32_t value);

    std::uint32_t getT() const;
    void setT(std::uint32_t value);

    std::uint64_t getF() const;
    void setF(std::uint64_t value);

    std::uint32_t getPC() const;
    void setPC(std::uint32_t value);

    std::uint32_t getSW() const;
    void setSW(std::uint32_t value);

    std::uint32_t read(RegisterId id) const;
    void write(RegisterId id, std::uint32_t value);

    ConditionCode getConditionCode() const;
    void setConditionCode(ConditionCode conditionCode);

    void reset();

private:
    using RegisterStorage = std::uint32_t RegisterFile::*;

    static constexpr std::uint64_t MASK_48 = 0xFFFFFFFFFFFF;

    static RegisterStorage storageFor(RegisterId id);

    std::uint32_t a_{};
    std::uint32_t x_{};
    std::uint32_t l_{};
    std::uint32_t b_{};
    std::uint32_t s_{};
    std::uint32_t t_{};
    std::uint32_t pc_{};
    std::uint32_t sw_{};

    std::uint64_t f_{};
};
