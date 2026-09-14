#pragma once

#include "AddressResolver.hpp"
#include "Decoder.hpp"
#include "Executor.hpp"
#include "Memory.hpp"
#include "RegisterFile.hpp"

class Machine
{
private:
    Memory memory_;
    RegisterFile registers_;
    Decoder decoder_;
    AddressResolver addressResolver_;
    Executor executor_;

public:
    Machine();

    void reset();
    void step();
};