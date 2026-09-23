# Operações aritméticas, comparações, shifts e indexação

Este documento descreve o módulo `src/core/execution/ArithmeticOperations.*`, que
implementa as 14 instruções do [pacote 3](work/03-arithmetic-operations.md). O módulo
consome operandos já resolvidos e produz os efeitos definidos nos
[contratos de execução](execution-contracts.md).

## Matriz das operações

| Instrução | Fonte aceita | Destino do efeito | Altera CC? | Descrição |
| --- | --- | --- | --- | --- |
| `ADD` | imediato ou memória | registrador `A` | Não | $A \leftarrow A + \text{op}$ |
| `ADDR` | registradores `r1, r2` | registrador `r2` | Não | $r_2 \leftarrow r_2 + r_1$ |
| `SUB` | imediato ou memória | registrador `A` | Não | $A \leftarrow A - \text{op}$ |
| `SUBR` | registradores `r1, r2` | registrador `r2` | Não | $r_2 \leftarrow r_2 - r_1$ |
| `MUL` | imediato ou memória | registrador `A` | Não | $A \leftarrow A \times \text{op}$ |
| `MULR` | registradores `r1, r2` | registrador `r2` | Não | $r_2 \leftarrow r_2 \times r_1$ |
| `DIV` | imediato ou memória | registrador `A` | Não | $A \leftarrow A / \text{op}$ |
| `DIVR` | registradores `r1, r2` | registrador `r2` | Não | $r_2 \leftarrow r_2 / r_1$ |
| `COMP` | imediato ou memória | nenhum | Sim | Compara $A$ com operando |
| `COMPR` | registradores `r1, r2` | nenhum | Sim | Compara $r_1$ com $r_2$ |
| `SHIFTL` | registrador e nibble | registrador `r1` | Não | Rotação circular de 24 bits à esquerda |
| `SHIFTR` | registrador e nibble | registrador `r1` | Não | Deslocamento aritmético à direita |
| `TIX` | imediato ou memória | registrador `X` | Sim | $X \leftarrow X + 1$, compara novo $X$ com operando |
| `TIXR` | registrador `r1` | registrador `X` | Sim | $X \leftarrow X + 1$, compara novo $X$ com $r_1$ |

## Semântica de 24 bits

- Padrões interpretados em complemento de dois de 24 bits no intervalo `-8388608..8388607`.
- Resultados são truncados/normalizados para 24 bits (`WordMask = 0x00FFFFFF`).
- Overflow comum produz wrap arquitetural nos 24 bits inferiores.
- Divisões truncam em direção a zero tanto para operandos positivos quanto negativos.
- Divisor zero (seja em memória/imediato ou registrador) retorna `ExecuteFaultCode::DivisionByZero` e nenhum efeito parcial.
- O caso limite de divisão `-8388608 / -1` é calculado em 64 bits para evitar *undefined behavior* em C++ e sofre wrap normalizado para `0x800000` (`-8388608`).

## Deslocamentos e Rotação

- A contagem efetiva de deslocamento é o nibble codificado mais um (`encodedNibble + 1`), variando de 1 a 16 posições.
- `SHIFTL` realiza rotação circular: os bits que saem pelo bit 23 entram pelo bit 0.
- `SHIFTR` realiza deslocamento aritmético com preservação e propagação do bit de sinal (bit 23).

## Indexação e Teste

- `TIX` e `TIXR` produzem um efeito duplo atômico: escrita do novo valor incrementado em `X` e atualização simultânea de `ConditionCode`.
- A comparação é feita utilizando o valor de `X` já incrementado.
- Nenhuma mutação de estado ocorre antes do commit da `Machine`.