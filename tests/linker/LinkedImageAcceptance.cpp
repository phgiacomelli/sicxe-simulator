#include "application/RawBinaryReader.hpp"
#include "core/Machine.hpp"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

std::optional<std::uint32_t> readAddress(std::string_view text) {
    if (!text.starts_with("0x")) {
        return std::nullopt;
    }
    text.remove_prefix(2);
    std::uint32_t value{};
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value, 16);
    return result.ec == std::errc{} && result.ptr == end ? std::optional{value} : std::nullopt;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        return 2;
    }
    const auto load = readAddress(argv[2]);
    const auto pc = readAddress(argv[3]);
    if (!load || !pc) {
        return 2;
    }
    const auto input = RawBinaryReader::read(argv[1], *load, *pc);
    const auto* program = std::get_if<ProgramInput>(&input);
    Machine machine;
    if (!program || !std::holds_alternative<LoadSuccess>(machine.loadProgram(
                        program->bytes, program->loadAddress, program->initialPc))) {
        std::cerr << "The linked image could not be loaded.\n";
        return 1;
    }
    for (int step = 0; step < 5; ++step) {
        if (!std::holds_alternative<StepSuccess>(machine.step())) {
            std::cerr << "Execution failed on step " << step + 1 << '\n';
            return 1;
        }
    }
    const auto state = machine.snapshot();
    if (state.registers.a != 14 || state.registers.l != 0x104 || state.registers.pc != 0x107 ||
        state.memory[0x113] != 0 || state.memory[0x114] != 0 || state.memory[0x115] != 14 ||
        state.lastFault) {
        std::cerr << "Unexpected registers or stored result after five steps.\n";
        return 1;
    }
    return 0;
}
