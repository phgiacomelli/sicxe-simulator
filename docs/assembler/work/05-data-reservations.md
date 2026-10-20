# Pacote 5 — Dados e reservas SIC/XE

Estado: implementado; validação local concluída, CI e revisão pendentes.

## Resultado esperado

Concentrar as convenções mínimas para colocar dados e espaço na memória
de **bytes** da VM: `BYTE`, `WORD`, `RESB` e `RESW`. A passagem 1 mede
e reserva; a passagem 2 produz bytes ou indicação de reserva, sempre com
o mesmo tamanho. Não criar pools de literais.

## Dependências e ownership

Consumir fonte/diagnóstico do pacote 1, números/rótulos do pacote 3 e
intermediário do pacote 4. Arquivos sugeridos:
`src/assembler/DataDirective.*`,
`tests/assembler/DataDirectiveTest.cpp`, `docs/assembler/data.md`.
Este PR integra o cálculo de dados/reservas em `PassOne` com revisão do
responsável pelo pacote 4; o algoritmo geral do contador continua lá.

## Interface a entregar

- `measureData(line, symbols)` devolve tamanho em bytes ou diagnóstico,
  sem emitir bytes. `WORD` mede sempre 3; reservas exigem quantidade
  inteira conhecida na passagem 1.
- `encodeData(locatedLine, symbols)` devolve bytes para `BYTE`/`WORD` ou
  marcação de reserva para `RESB`/`RESW`; o tamanho deve coincidir com
  o medido. Um `WORD rótulo` usa o endereço **fixo** resolvido pela tabela.
- Reservas não recebem bytes no intermediário: a saída `.bin` do pacote 7
  as preenche com zero somente ao serializar a imagem para a VM.

## Comportamento obrigatório

`BYTE C'...'` codifica ASCII byte a byte; `BYTE X'...'` exige quantidade
par de dígitos hexadecimais. `WORD` codifica um inteiro ou endereço de
rótulo em 24 bits, ordem big-endian; inteiros permitidos vão de
`-0x800000` a `0xFFFFFF`, com negativos em complemento de dois.
`RESB n` reserva `n` bytes; `RESW n` reserva `3n` bytes; `n` não pode ser
negativo nem expressão/rótulo futuro. Validar comprimento antes de
alterar contador, sem wrap. Nenhum desses comandos é opcode da VM:
são apenas convenções de fonte para emitir dados ou espaço.

## Testes obrigatórios

- `C`/`X` normais, string vazia, hex ímpar e caractere inválido;
- `WORD` zero, limites, negativo, excesso e rótulo futuro;
- `RESB`/`RESW` zero, valores normais, negativo e overflow;
- tamanho de passagem 1 igual ao produzido na passagem 2;
- rótulos após reservas no endereço correto;
- nenhuma reserva gera opcode ou bytes inesperados antes do writer.

## Fora do escopo

Não codificar instruções, escolher PC/base, criar `CONST`/`SPACE` da
máquina didática, `EQU`/`ORG`, literais, registros de relocação ou
constantes floating point.

## Critério de conclusão

- [x] Dados e reservas se integram ao contador sem reparsear a linha.
- [x] Bytes, tamanhos, limites e falhas cobertos por testes públicos.
- [x] `docs/assembler/data.md` registra sintaxe e codificação.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Entregues `DataDirective.hpp/.cpp`, testes públicos e integração de `measureData`
ao contador do pacote 4. Sintaxe, tipos de resultado e diagnósticos estão em
[data.md](../data.md). A implementação segue as convenções mínimas do dialeto
já contratado, sem acrescentar interpretações dos PDFs distribuídos fora do Git.

Decisões e interfaces:

- Um planejamento privado comum mede e valida os operandos nas duas operações;
  a medição não produz bytes. Não se reabre nem se reanalisa a linha de fonte.
- `WORD` usa `parseValue`/`validateInteger`/`referenceOperand` na primeira
  passagem e `resolveOperand` na codificação, preservando referências futuras.
- Reservas usam `parseKnownInteger`, com verificação antes de multiplicar;
  `DataReservation` contém tamanho, sem vetor de zeros. O preenchimento `.bin`
  fica no pacote 7.
- Nenhuma assinatura ou campo público dos pacotes 1–4 foi alterado. A integração
  usa `InvalidDirectiveOperand`/`AddressOutOfRange` já existentes em `PassOne`;
  a API de dados preserva detalhes tipados do pacote 3.
- Fora do ownership principal, foram atualizados `PassOne.cpp`, seu teste,
  CMake e documentos de estado/contratos. A integração requer revisão do
  responsável pelo pacote 4 antes do merge.

Testes adicionados cobrem ASCII/hex, vazios, WORD signed/unsigned de 24 bits,
referências anteriores/futuras, indefinidos, reservas zero/máximas, overflow,
consistência de tamanhos e limites do contador. Resultados efetivos de build,
formatação e CTest: builds Release em diretórios novos passaram em **373 testes
com Qt** e **370 sem Qt**, usando GCC 16.2.1 e GoogleTest 1.17.0. A fonte do
GoogleTest obtida na configuração completa foi reutilizada no build sem Qt via
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`. Formatação pelo alvo `format` e verificação
`clang-format --dry-run --Werror` executadas em container Fedora 43 com
clang-format 21; `git diff --check` também passou. Nenhum teste anterior falhou.
CI e revisão externa ainda precisam ser confirmadas antes do merge.
Passagem 2 completa, writer e codificação de instruções continuam fora deste pacote.
