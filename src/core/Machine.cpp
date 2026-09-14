#include "Machine.hpp"

namespace {
constexpr std::size_t SicXeMemorySize = 1U << 20;
}

Machine::Machine() : memory_(SicXeMemorySize) {
}

void Machine::reset() {
}

void Machine::step() {
}
