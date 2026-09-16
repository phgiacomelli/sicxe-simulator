#include "Memory.hpp"

#include <stdexcept>

Memory::Memory(std::size_t size) : data_(size, 0) {
}

std::uint8_t Memory::readByte(std::size_t address) const {
    validateAddress(address);
    return data_[address];
}

void Memory::writeByte(std::size_t address, std::uint8_t value) {
    validateAddress(address);

    data_[address] = value;
}

std::uint32_t Memory::readWord(std::size_t address) const {
    validateAddress(address, 3);

    return (static_cast<std::uint32_t>(data_[address]) << 16) |
           (static_cast<std::uint32_t>(data_[address + 1]) << 8) |
           static_cast<std::uint32_t>(data_[address + 2]);
}

void Memory::writeWord(std::size_t address, std::uint32_t value) {
    validateAddress(address, 3);

    value &= 0xFFFFFF;

    data_[address] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data_[address + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data_[address + 2] = static_cast<std::uint8_t>(value & 0xFF);
}

std::size_t Memory::size() const {
    return data_.size();
}

void Memory::validateAddress(std::size_t address, std::size_t bytes) const {
    if (address + bytes > data_.size()) {
        throw std::out_of_range("Memory access out of bounds");
    }
}