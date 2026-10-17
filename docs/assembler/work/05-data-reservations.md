# Pacote 5 — Dados e reservas SIC/XE

Estado: planejado. Responsável: a definir.

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

- [ ] Dados e reservas se integram ao contador sem reparsear a linha.
- [ ] Bytes, tamanhos, limites e falhas cobertos por testes públicos.
- [ ] `docs/assembler/data.md` registra sintaxe e codificação.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Preencher ao implementar: decisões, divergências dos PDFs, interfaces
alteradas, testes e pendências.
