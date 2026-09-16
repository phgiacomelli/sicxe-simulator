#include "RegisterFile.hpp"

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