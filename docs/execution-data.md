# Operações de transferência e lógica

Este documento descreve o módulo `src/core/execution/DataOperations.*`, que
implementa as 18 instruções do [pacote 2](work/02-data-operations.md). O módulo
consome operandos já resolvidos conforme o
[guia de integração](work/02-data-operations-integration.md) e produz os efeitos
definidos nos [contratos de execução](execution-contracts.md).

## Matriz fonte, destino e largura

| Instrução | Fonte aceita | Destino do efeito | Largura |
| --- | --- | --- | --- |
| `CLEAR` | registrador `r1` | registrador `r1` | 24 bits |
| `RMO` | registrador `r1` | registrador `r2` | 24 bits |
| `LDA/LDB/LDL/LDS/LDT/LDX` | imediato ou memória | registrador definido pelo opcode | 24 bits |
| `STA/STB/STL/STS/STT/STX` | registrador definido pelo opcode | memória | palavra de 3 bytes |
| `LDCH` | imediato ou byte de memória | 8 bits inferiores de `A` | 8 bits |
| `STCH` | 8 bits inferiores de `A` | memória | 1 byte |
| `AND/OR` | imediato ou palavra de memória | registrador `A` | 24 bits |

`LDCH` preserva os 16 bits superiores de `A`. `STCH` produz somente uma escrita
de byte. Loads e operações lógicas leem o valor imediato diretamente ou usam o
endereço de memória recebido; stores aceitam apenas `MemoryAddress`.

## Efeitos e falhas

Todas as operações preservam o `postFetchPc` recebido e não alteram o Condition
Code. `CLEAR` e o destino de `RMO` rejeitam `SW` para manter essa garantia.

As funções apenas leem `Memory` e `RegisterFile`. Escritas são descritas em
`ExecutionEffect` e serão validadas e confirmadas posteriormente pela
`Machine`. Operandos incompatíveis produzem `InvalidOperand`; leituras fora dos
limites da memória produzem `MemoryAccessOutOfRange`. Nenhuma falha carrega um
efeito parcial.
