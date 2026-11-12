#include "MacroAssemblerCli.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    for (int index = 1; index < argc; ++index)
        arguments.emplace_back(argv[index]);
    return macro::runMacroAssemblerCli(arguments, std::cout, std::cerr);
}
