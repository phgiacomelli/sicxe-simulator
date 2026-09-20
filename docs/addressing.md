# Resolução de operandos

## Responsabilidade

`AddressResolver` transforma uma `DecodedInstruction` em um operando pronto
para ser consumido pelo executor. Ele aplica registradores de endereçamento,
extensão de sinal, indexação e indireção, mas não executa o opcode e não altera
memória, registradores ou PC.

Sua interface pública é:

```cpp
ResolveResult resolve(const DecodedInstruction& instruction,
                      std::uint32_t postFetchPc,
                      const Memory& memory,
                      const RegisterFile& registers) const;
```

`postFetchPc` é o endereço imediatamente posterior à instrução. A `Machine`
será responsável por calculá-lo antes de chamar o resolver.

## Tipos compartilhados

`Architecture.hpp` define os identificadores de registradores inteiros aceitos
pelo decoder: `A`, `X`, `L`, `B`, `S`, `T`, `PC` e `SW`. O registrador `F`
continua armazenado pelo `RegisterFile`, porém não participa dos operandos do
conjunto atual de instruções.

`Operand.hpp` define seis resultados bem-sucedidos:

| Tipo | Significado |
| --- | --- |
| `NoOperand` | instrução sem operando, atualmente `RSUB` |
| `SingleRegisterOperand` | um registrador do formato 2 |
| `RegisterPairOperands` | dois registradores do formato 2 |
| `RegisterAndNibbleOperand` | registrador e nibble codificado de shift |
| `ImmediateValue` | valor calculado, sem leitura final de memória |
| `MemoryAddress` | localização que poderá ser lida ou escrita pelo executor |

O resultado da resolução contém um `ResolvedOperand` ou um `ResolveFault`.

## Registradores e Condition Code

`RegisterFile::read` e `RegisterFile::write` fornecem acesso por `RegisterId` e
aplicam a mesma normalização de 24 bits dos acessores individuais.

O Condition Code usa os dois bits menos significativos de `SW`:

| Condição | Codificação |
| --- | --- |
| `Equal` | `00` |
| `Greater` | `01` |
| `Less` | `10` |

`11` é reservado e sua leitura como Condition Code é rejeitada. Ao atualizar o
CC, todos os outros bits de `SW` são preservados.

## Formato 2

Os códigos de `r1` e `r2` são convertidos em `RegisterId` antes de chegar ao
executor.

- `CLEAR` e `TIXR` produzem somente `r1`;
- `ADDR`, `COMPR`, `DIVR`, `MULR`, `RMO` e `SUBR` produzem `r1` e `r2`;
- `SHIFTL` e `SHIFTR` produzem `r1` e preservam o segundo nibble como valor
  codificado. O ajuste da contagem pertence à execução.

Embora o decoder já valide esses campos, o resolver também devolve
`InvalidRegister` quando recebe uma instrução construída com código reservado.

## Cálculo principal

O cálculo usa um tipo mais largo do que os campos arquiteturais para impedir
wrap silencioso.

### SIC

O campo unsigned de 15 bits é o endereço inicial. Quando `x=1`, o valor de `X`
é somado depois desse cálculo.

### Formato 3

- direto: o campo unsigned de 12 bits é usado sem base;
- relativo ao PC: o campo de 12 bits recebe extensão de sinal e é somado a
  `postFetchPc`;
- relativo à base: o deslocamento unsigned é somado a `B`.

Um deslocamento PC-relative entre `0x800` e `0xFFF` representa respectivamente
os valores negativos de `-2048` a `-1`.

### Formato 4

O campo unsigned de 20 bits é usado diretamente. As combinações inválidas com
`b` ou `p` já são rejeitadas pelo decoder.

### Indexação

Quando presente, `X` é somado depois do cálculo principal. O decoder restringe
indexação ao modo simples.

## Aplicação do modo

Depois do cálculo e da validação de limites:

- imediato devolve o valor calculado e não lê a localização final;
- simples devolve a localização calculada;
- indireto lê uma palavra big-endian de 24 bits na localização calculada e usa
  essa palavra como endereço final.

A localização usada para buscar um ponteiro indireto precisa comportar os três
bytes da palavra. Tanto essa localização quanto o endereço contido nela devem
pertencer à memória.

## Falhas

| Código | Significado |
| --- | --- |
| `InvalidInstruction` | formato, opcode ou largura de campo incompatível |
| `InvalidRegister` | código de registrador não aceito pelo projeto |
| `AddressOutOfRange` | resultado negativo ou fora da memória |
| `IndirectReadOutOfRange` | os três bytes do ponteiro cruzariam o fim da memória |

`ResolveFault::offendingValue` preserva, quando aplicável, o valor que causou a
falha. Isso inclui códigos de registrador inválidos e endereços negativos
produzidos por deslocamento PC-relative.

## Exemplos

Com `postFetchPc=0x1000`, o campo PC-relative `0xFF0` representa `-16` e resulta
em `0x0FF0`.

Com `B=0x2000` e deslocamento `0x120`, o endereço base-relative é `0x2120`.

No modo indireto, se o endereço calculado for `0x0100` e os bytes nessa posição
forem `00 20 00`, o operando resolvido é `MemoryAddress{0x2000}`.
