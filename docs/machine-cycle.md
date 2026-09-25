# Ciclo da `Machine`

## Responsabilidade

A `Machine` integra `Decoder`, `AddressResolver` e `Executor` em um ciclo atômico
e é a única dona das escritas no estado arquitetural (memória, registradores e
PC). Nenhum dos outros módulos altera a VM: cada um lê o estado e devolve um
resultado tipado.

```text
Memory / RegisterFile
  ↓ leitura
Decoder → AddressResolver → Executor → validação do efeito
                                          ↓ efeito válido
                                       Machine: commit
```

Os tipos de falha estão em `src/core/MachineFault.hpp`. A validação do efeito
está em `src/core/EffectValidation.*`. As operações de controle de fluxo estão
em `src/core/execution/ControlOperations.*`.

## Interface pública

```cpp
explicit Machine(std::size_t memorySize = Machine::DefaultMemorySize); // 1 MiB

LoadResult loadProgram(std::span<const std::uint8_t> bytes,
                       std::uint32_t loadAddress,
                       std::uint32_t initialPc);
void reset();
StepResult step();
MachineSnapshot snapshot() const;
```

O construtor padrão cria exatamente 1 MiB de memória. O tamanho configurável
existe para testes de limites; a aplicação usa a composição padrão.

### `loadProgram`

Copia bytes contíguos para a memória a partir de `loadAddress` e define o PC
inicial. Todas as validações ocorrem antes da primeira alteração de estado, na
ordem abaixo. A primeira que falhar é devolvida como `LoadFault`.

| Código | Condição | `offendingValue` |
| --- | --- | --- |
| `EmptyProgram` | nenhum byte | ausente |
| `LoadAddressOutOfRange` | `loadAddress >= tamanho da memória` | `loadAddress` |
| `ProgramExceedsMemory` | `loadAddress + bytes > tamanho da memória` | endereço final exclusivo |
| `InitialPcOutOfRange` | `initialPc` fora da memória ou acima de 24 bits | `initialPc` |

Uma carga rejeitada não altera memória, registradores, PC nem a última falha.
Uma carga válida primeiro executa o mesmo zeramento de `reset`, depois copia os
bytes e por fim grava o PC inicial. Portanto, nada do programa anterior
sobrevive. O PC inicial não precisa estar dentro da faixa carregada; precisa
apenas apontar para um byte existente da memória.

### `reset`

Zera toda a memória (mantendo o tamanho), zera `A`, `X`, `L`, `B`, `S`, `T`,
`F`, `PC` e `SW` e remove a última falha. Após o reset o Condition Code é
`Equal`, pois a codificação `00` de `SW` representa igualdade.

### `snapshot`

Devolve uma cópia imutável do estado:

- `registers`: `a`, `x`, `l`, `b`, `s`, `t`, `f`, `pc`, `sw` e `conditionCode`.
  O CC é opcional porque a codificação reservada `11` não pode ser lida como
  condição; `sw` continua disponível nesse caso;
- `memory`: todos os bytes da memória;
- `lastFault`: falha do último `step`, se houver.

O snapshot copia a memória inteira. Ele não muda quando a máquina muda depois
dele. `RegisterSnapshot` possui `operator==`, o que permite comparar estados
antes e depois de uma tentativa.

### `step`

Executa exatamente um ciclo e devolve `StepResult`:

- `StepSuccess`: instrução decodificada, `ExecutionEffect` confirmado e o PC
  resultante (`nextPc`);
- `MachineFault`: `instructionAddress` (PC da tentativa) e a causa.

Uma falha é gravada como `lastFault`; um sucesso a remove. Como uma falha não
altera o estado, repetir `step` sem carregar nem resetar produz a mesma falha.

## Etapas do ciclo

| Etapa | Ação | Quem executa | Pode falhar com |
| --- | --- | --- | --- |
| 1 | Capturar o PC da instrução | `Machine` | — |
| 2 | Decodificar, sem alterar estado | `Decoder` | `DecodeFault` |
| 3 | Calcular o PC pós-fetch em tipo largo | `computePostFetchPc` | `CycleFault` |
| 4 | Resolver o operando | `AddressResolver` | `ResolveFault` |
| 5 | Executar e produzir um efeito | `Executor` | `ExecuteFault` |
| 6 | Validar todos os destinos do efeito | `validateEffect` | `CycleFault` |
| 7 | Confirmar memória, registradores, CC e PC | `Machine` | — |

### Fetch e decode

O fetch é a leitura dos até quatro bytes a partir do PC, feita pelo `Decoder`.
Ele reconhece as 38 instruções do projeto nos formatos SIC, 2, 3 e 4 e devolve
`DecodedInstruction` ou `DecodeFault` (`UnknownOpcode`, `TruncatedInstruction`,
`InvalidRegister`, `InvalidFlagCombination`, `InvalidOperandEncoding`). Não há
instruções de formato 1 no conjunto; um opcode de formato 1 é opcode
desconhecido.

### PC pós-fetch

A `Machine` chama `computePostFetchPc(instructionAddress, length)` uma única
vez. A regra não é reimplementada em nenhum outro módulo: o resolver e o
executor recebem o valor pronto. O cálculo é feito em tipo largo e não faz wrap;
se ultrapassar 24 bits, o ciclo falha com `PostFetchPcOutOfRange`. Com a
memória padrão de 1 MiB isso não ocorre, porque o decoder já falha antes; o
caso é alcançável apenas em memórias maiores que 16 MiB.

Avanço por formato: SIC e formato 3 avançam 3 bytes, formato 2 avança 2 bytes e
formato 4 avança 4 bytes.

### Resolução do operando

O `AddressResolver` recebe a instrução, o PC pós-fetch, a memória e os
registradores e devolve um `ResolvedOperand` (`NoOperand`, registradores,
`ImmediateValue` ou `MemoryAddress`) ou um `ResolveFault` (`InvalidInstruction`,
`InvalidRegister`, `AddressOutOfRange`, `IndirectReadOutOfRange`). Toda a
lógica de `n/i/x/b/p/e`, extensão de sinal, indexação e indireção pertence a ele.

### Execução

O `Executor` seleciona a família pelo opcode e delega a ela:

| Família | Função | Opcodes |
| --- | --- | --- |
| Transferência e lógica | `executeDataOperation` | 18 |
| Aritmética | `executeArithmeticOperation` | 14 |
| Controle de fluxo | `executeControlOperation` | 6 |

Cada função devolve um `ExecuteResult`: um `ExecutionEffect` completo ou um
`ExecuteFault`. Elas apenas leem memória e registradores. Um opcode fora das
três famílias produz `UnsupportedOpcode`.

### Validação do efeito

`validateEffect` examina o efeito inteiro sem alterar estado e devolve a
primeira falha, na ordem:

| Ordem | Verificação | Código |
| --- | --- | --- |
| 1 | identificador do registrador escrito é um `RegisterId` conhecido | `InvalidRegisterWrite` |
| 2 | Condition Code é `Less`, `Equal` ou `Greater` | `InvalidConditionCode` |
| 3 | não há escrita em `PC` com `nextPc` diferente do PC pós-fetch, nem escrita em `SW` junto com atualização de CC | `ConflictingEffect` |
| 4 | a escrita de memória cabe inteira (1 byte, ou 3 bytes para palavra) | `MemoryWriteOutOfRange` |
| 5 | `nextPc` é um PC válido | `NextPcOutOfRange` |
| 6 | o PC que o efeito deixa (ver [escrita em PC](#decisões-do-ciclo)) é um PC válido | `NextPcOutOfRange` |

Um PC é válido quando aponta para um byte existente da memória e cabe em 24
bits. Os passos 5 e 6 são independentes de propósito: uma escrita em `PC` não
pode esconder um `nextPc` (e, portanto, um PC pós-fetch) fora da memória.

### Commit

Só ocorre depois de todas as etapas anteriores. A ordem é memória, registrador
(exceto `PC`), Condition Code e por fim `PC`. Como o efeito foi validado e os
conflitos foram excluídos, essas escritas não dependem de ordem e não podem
falhar.

## Atomicidade

Enquanto o ciclo está nas etapas 2 a 6 nenhum módulo escreve na VM: o
`Decoder`, o `AddressResolver`, o `Executor` e a validação só recebem
referências constantes. O único trecho que escreve é o commit, e ele só é
alcançado com um efeito totalmente validado. Portanto, qualquer falha nas etapas
2 a 6 deixa memória, registradores e PC exatamente como estavam. A única
informação atualizada em uma falha é `lastFault`, que é diagnóstica e não faz
parte do estado arquitetural. Os testes de atomicidade capturam um `snapshot`
antes, executam `step`, confirmam a falha e comparam registradores, PC e
memória depois.

## Tratamento de falhas

`MachineFault` carrega o endereço da instrução e uma variante com a causa,
sempre estruturada; a conversão para texto pertence à interface.

| Causa | Origem | Códigos |
| --- | --- | --- |
| `DecodeFault` | `Decoder` | `UnknownOpcode`, `TruncatedInstruction`, `InvalidRegister`, `InvalidFlagCombination`, `InvalidOperandEncoding` |
| `ResolveFault` | `AddressResolver` | `InvalidInstruction`, `InvalidRegister`, `AddressOutOfRange`, `IndirectReadOutOfRange` |
| `ExecuteFault` | `Executor` | `UnsupportedOpcode`, `InvalidOperand`, `DivisionByZero`, `MemoryAccessOutOfRange` |
| `CycleFault` | `Machine` | `PostFetchPcOutOfRange`, `NextPcOutOfRange`, `InvalidRegisterWrite`, `InvalidConditionCode`, `MemoryWriteOutOfRange`, `ConflictingEffect` |

Exemplos: um `STA` cujo endereço resolvido é `0xFFFFE` passa pelo resolver (o
endereço existe), mas a palavra de 3 bytes não cabe na memória; a falha é
`MemoryWriteOutOfRange`. Um `RSUB` com `L` fora da memória falha com
`NextPcOutOfRange`, pois nenhum outro módulo valida o conteúdo de `L`.

## Controle de fluxo

Todas as operações usam o PC pós-fetch recebido; nenhuma recalcula o PC.

| Instrução | Efeito |
| --- | --- |
| `J` | `nextPc` recebe o endereço resolvido |
| `JEQ` | se CC é `Equal`, `nextPc` recebe o destino; senão permanece o PC pós-fetch |
| `JGT` | igual a `JEQ`, com CC `Greater` |
| `JLT` | igual a `JEQ`, com CC `Less` |
| `JSUB` | `L` recebe o PC pós-fetch e `nextPc` recebe o destino |
| `RSUB` | `nextPc` recebe o valor de `L` |

Detalhes:

- o destino é o endereço entregue pelo resolver, tanto em `MemoryAddress`
  (simples e indireto, já dereferenciado) quanto em `ImmediateValue`; qualquer
  outro operando produz `InvalidOperand`;
- `RSUB` exige `NoOperand`;
- o operando é validado antes de consultar o CC;
- um salto não tomado não produz nenhuma escrita, apenas o PC sequencial;
- se `SW` contém a codificação reservada `11`, `JEQ`, `JGT` e `JLT` falham com
  `InvalidOperand` (`offendingValue` igual a 3), porque não há condição a
  avaliar. `J`, `JSUB` e `RSUB` não dependem do CC;
- a operação não valida se o destino existe na memória: isso pertence à
  validação do efeito. Os destinos vindos do resolver já estão na memória; o
  valor de `L` em `RSUB` não está garantido.

## Decisões do ciclo

### Escrita em `PC`

Algumas operações de formato 2 aceitam `PC` como registrador de destino
(`CLEAR`, `RMO`, `ADDR`, `SUBR`, `MULR`, `DIVR`, `SHIFTL`, `SHIFTR`). O efeito
delas carrega o `nextPc` sequencial e uma `registerWrite` com alvo `PC`.

Decisão: uma escrita de registrador em `PC` é uma transferência de controle. O
PC final é o valor escrito; a escrita não é aplicada duas vezes e não depende da
ordem do commit. Para haver uma única representação, o efeito só é aceito se o
`nextPc` for o PC pós-fetch; um efeito com escrita em `PC` e outro `nextPc` é
ambíguo e falha com `ConflictingEffect`. Tanto o `nextPc` quanto o PC final são
validados.

Alternativa considerada: rejeitar toda escrita em `PC` por formato 2. Foi
descartada porque o decoder, o resolver e os pacotes 2 e 3 já aceitam `PC` como
destino, e recusá-lo mudaria o comportamento daquelas instruções.

Consequência da regra de PC válido: uma instrução que termina exatamente no
último byte da memória e segue em sequência falha com `NextPcOutOfRange` (o PC
sequencial não apontaria para nenhum byte), enquanto um salto a partir dali para
um endereço existente é aceito.

### `PC` e `SW` em instruções de formato 2

- Leitura de `PC`: as instruções veem o PC pós-fetch, como no cálculo
  PC-relative e como em `JSUB`. Por isso `RMO PC,L` grava em `L` o endereço da
  instrução seguinte. A `Machine` passa ao resolver e ao executor uma cópia do
  `RegisterFile` com esse PC; o registrador real só muda no commit.
- Escrita em `PC`: descrita acima.
- Escrita em `SW`: `CLEAR` e o destino de `RMO`, `ADDR`, `SUBR`, `MULR`, `DIVR`,
  além do registrador de `SHIFTL`/`SHIFTR`, rejeitam `SW` com `InvalidOperand`,
  o que preserva o Condition Code. Se algum efeito futuro escrever `SW` como
  registrador, a escrita é aceita sozinha; combinada com uma atualização de CC
  é `ConflictingEffect`, pois ambas alteram os bits inferiores de `SW`.
- Condition Code: atualizado somente por `conditionCode`; a atualização
  preserva os demais bits de `SW`.

### Execução contínua

`Machine` expõe somente `step()`. Executar continuamente e pausar é
responsabilidade do adaptador do [pacote 5](work/05-qt-adapter.md), que
repete `step()` e pode pausar entre instruções, pois cada `step()` é atômico.

## Testes

- `tests/ControlOperationsTest.cpp`: as seis operações isoladas.
- `tests/EffectValidationTest.cpp`: validação e efeitos malformados.
- `tests/ExecutorTest.cpp`: dispatcher e cobertura dos 38 opcodes.
- `tests/MachineTest.cpp`: integração com `Decoder`, `AddressResolver`,
  `Executor` e `Machine` reais: carga, reset, PC por formato, saltos, `JSUB` e
  `RSUB`, as 38 instruções pelo ciclo completo, falhas, atomicidade e snapshots.
