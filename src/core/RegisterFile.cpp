#include "RegisterFile.hpp"

#include <stdexcept>

RegisterFile::RegisterFile() {
    reset();
}

std::uint32_t RegisterFile::getA() const {
    return a_;
}

void RegisterFile::setA(std::uint32_t value) {
    a_ = value & MASK_24;
}

std::uint32_t RegisterFile::getX() const {
    return x_;
}

void RegisterFile::setX(std::uint32_t value) {
    x_ = value & MASK_24;
}

std::uint32_t RegisterFile::getL() const {
    return l_;
}

void RegisterFile::setL(std::uint32_t value) {
    l_ = value & MASK_24;
}

std::uint32_t RegisterFile::getB() const {
    return b_;
}

void RegisterFile::setB(std::uint32_t value) {
    b_ = value & MASK_24;
}

std::uint32_t RegisterFile::getS() const {
    return s_;
}

void RegisterFile::setS(std::uint32_t value) {
    s_ = value & MASK_24;
}

std::uint32_t RegisterFile::getT() const {
    return t_;
}

void RegisterFile::setT(std::uint32_t value) {
    t_ = value & MASK_24;
}

std::uint64_t RegisterFile::getF() const {
    return f_;
}

void RegisterFile::setF(std::uint64_t value) {
    f_ = value & MASK_48;
}

std::uint32_t RegisterFile::getPC() const {
    return pc_;
}

void RegisterFile::setPC(std::uint32_t value) {
    pc_ = value & MASK_24;
}

std::uint32_t RegisterFile::getSW() const {
    return sw_;
}

void RegisterFile::setSW(std::uint32_t value) {
    sw_ = value & MASK_24;
}

std::uint32_t RegisterFile::read(RegisterId id) const {
    switch (id) {
        case RegisterId::A:
            return getA();
        case RegisterId::X:
            return getX();
        case RegisterId::L:
            return getL();
        case RegisterId::B:
            return getB();
        case RegisterId::S:
            return getS();
        case RegisterId::T:
            return getT();
        case RegisterId::PC:
            return getPC();
        case RegisterId::SW:
            return getSW();
    }

    throw std::invalid_argument("Unknown register identifier");
}

void RegisterFile::write(RegisterId id, std::uint32_t value) {
    switch (id) {
        case RegisterId::A:
            setA(value);
            return;
        case RegisterId::X:
            setX(value);
            return;
        case RegisterId::L:
            setL(value);
            return;
        case RegisterId::B:
            setB(value);
            return;
        case RegisterId::S:
            setS(value);
            return;
        case RegisterId::T:
            setT(value);
            return;
        case RegisterId::PC:
            setPC(value);
            return;
        case RegisterId::SW:
            setSW(value);
            return;
    }

    throw std::invalid_argument("Unknown register identifier");
}

ConditionCode RegisterFile::getConditionCode() const {
    switch (sw_ & ConditionCodeMask) {
        case 0b00:
            return ConditionCode::Equal;
        case 0b01:
            return ConditionCode::Greater;
        case 0b10:
            return ConditionCode::Less;
        default:
            throw std::logic_error("Invalid condition code encoding");
    }
}

void RegisterFile::setConditionCode(ConditionCode conditionCode) {
    std::uint32_t encoded = 0;
    switch (conditionCode) {
        case ConditionCode::Less:
            encoded = 0b10;
            break;
        case ConditionCode::Equal:
            encoded = 0b00;
            break;
        case ConditionCode::Greater:
            encoded = 0b01;
            break;
    }

    sw_ = (sw_ & ~ConditionCodeMask) | encoded;
}

void RegisterFile::reset() {
    a_ = 0;
    x_ = 0;
    l_ = 0;
    b_ = 0;
    s_ = 0;
    t_ = 0;
    f_ = 0;
    pc_ = 0;
    sw_ = 0;
}
