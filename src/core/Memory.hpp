#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

class Memory {
public:
    explicit Memory(std::size_t size);

    std::uint8_t readByte(std::size_t address) const;
    void writeByte(std::size_t address, std::uint8_t value);

    std::uint32_t readWord(std::size_t address) const;
    void writeWord(std::size_t address, std::uint32_t value);

    std::size_t size() const;

private:
    std::vector<std::uint8_t> data_;

    void validateAddress(std::size_t address, std::size_t bytes = 1) const;
};