#include "RawBinaryReader.hpp"

#include <fstream>

ProgramInputResult RawBinaryReader::read(const std::filesystem::path& path,
                                         std::uint32_t loadAddress,
                                         std::uint32_t initialPc) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        return InputFault{InputFaultCode::OpenFailed};
    return read(stream, loadAddress, initialPc);
}

ProgramInputResult
RawBinaryReader::read(std::istream& stream, std::uint32_t loadAddress, std::uint32_t initialPc) {
    ProgramInput program{{}, loadAddress, initialPc};
    try {
        char byte;
        while (stream.get(byte))
            program.bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    } catch (const std::ios_base::failure&) {
        // Streams may be configured to throw even on normal EOF.
        if (stream.bad() || !stream.eof())
            return InputFault{InputFaultCode::ReadFailed};
    }
    if (stream.bad() || !stream.eof())
        return InputFault{InputFaultCode::ReadFailed};
    if (program.bytes.empty())
        return InputFault{InputFaultCode::EmptyInput};
    return program;
}
