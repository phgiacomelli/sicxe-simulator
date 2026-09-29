#include "InputFaultText.hpp"

QString inputFaultMessage(const InputFault& fault) {
    QString message;
    switch (fault.code) {
        case InputFaultCode::EmptyInput:
            message = "Entrada vazia";
            break;
        case InputFaultCode::InvalidDigit:
            message = "Dígito hexadecimal inválido";
            break;
        case InputFaultCode::IncompleteByte:
            message = "Byte incompleto";
            break;
        case InputFaultCode::ByteOutOfRange:
            message = "Byte longo demais";
            break;
        case InputFaultCode::OpenFailed:
            message = "Não foi possível abrir o arquivo";
            break;
        case InputFaultCode::ReadFailed:
            message = "Falha na leitura do arquivo";
            break;
    }
    if (fault.offset)
        message += QString(" na posição %1").arg(*fault.offset);
    return message + '.';
}
