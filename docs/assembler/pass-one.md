# Passagem 1 e contador de localização

O pacote 4 está implementado em `src/assembler/`, no namespace `assembler`.
`PassOne` percorre as linhas sintaticamente válidas produzidas pelo
`SourceParser` **uma única vez**, mantém o contador de localização em
**bytes**, registra definições e referências de rótulos locais, valida as
diretivas estruturais (`START`, `END`, `BASE` e `NOBASE`) e materializa o
intermediário imutável (`LocatedLine` e `PassOneResult`) para a passagem 2.

## Interface pública

```cpp
#include "assembler/PassOne.hpp"
#include "assembler/SourceParser.hpp"

const auto parsed = assembler::SourceParser::parse(
    "       START 0x1000\n"
    "FIRST  CLEAR A\n"
    "       +JSUB CALLEE\n"
    "CALLEE RSUB\n"
    "       END   FIRST\n");
const auto passOne = assembler::PassOne::run(parsed);
```

- `PassOne::run(const SourceParseResult&)` consome as `SourceLine` somente
  quando `parseResult.succeeded()` for verdadeiro; se houver erro sintático no
  parser, devolve `PassOneResult` com diagnóstico `InvalidSourceLine` sem
  tentar montar parcialmente.
- `PassOne::run(std::span<const SourceLine>)` aceita diretamente uma sequência
  de `SourceLine` válidas e rejeita linhas `Invalid`.
- `PassOneResult` reúne:
  - `startAddress`: endereço inicial fixado por `START` (ou `0` na ausência de
    `START`);
  - `initialPc`: endereço de entrada fixado pelo rótulo de `END` (ou
    `startAddress` quando `END` não informa operando);
  - `programLength`: extensão total em bytes (`locationCounter - startAddress`);
  - `lines`: `std::vector<LocatedLine>` na ordem física original da fonte;
  - `symbols`: `std::vector<AssemblySymbol>` em ordem determinística de
    definição;
  - `diagnostics`: `std::vector<PassOneDiagnostic>` e `succeeded()`.

## Representação do intermediário (`LocatedLine`)

Cada linha física origina um `LocatedLine` imutável com:

| Campo | Conteúdo |
| --- | --- |
| `source` | `SourceLine` original com número físico (1-based), texto, declaração e comentário |
| `category` | `Instruction`, `Data`, `Reservation`, `Directive` ou `Comment` |
| `address` | Endereço em bytes atribuído antes do avanço do contador |
| `size` | Tamanho fixo em bytes (`2`, `3` ou `4` em instruções; `0` em diretivas e comentários) |
| `mnemonic` | Mnemônico ou diretiva normalizada em maiúsculas sem o prefixo `+` |
| `extended` | `true` somente quando a instrução foi escrita com prefixo `+` (formato 4) |
| `opcode` | Opcode base das 38 instruções reconhecidas pela VM (`std::nullopt` fora de instrução) |
| `instructionFormat` | `Format2` (2B), `Format3` (3B) ou `Format4` (4B) para instruções |
| `directive` | `Start`, `End`, `Base`, `NoBase`, `Byte`, `Word`, `Resb` ou `Resw` |

`LocatedLine::listingAddress()` retorna `std::optional<std::uint32_t>` com o
endereço nas linhas que alocam posição ou fixam `START` (`Instruction`, `Data`,
`Reservation` e `START`), e `std::nullopt` em comentários, linhas em branco,
`BASE`, `NOBASE` e `END`.

## Exemplo de intermediário SIC/XE real

Entrada assembly:

```text
1: . Exemplo SIC/XE de passagem 1
2: PROG   START  0x1000
3: FIRST  CLEAR  X
4:        LDB    #CALLEE
5:        BASE   CALLEE
6: LOOP   LDA    #5
7:        +JSUB  CALLEE
8:        ADDR   A,X
9:        NOBASE
10: CALLEE RSUB
11:        END    FIRST
```

Intermediário `PassOneResult` produzido:

- `startAddress = 0x001000`
- `initialPc = 0x001000` (`FIRST`)
- `programLength = 17` bytes (`0x11`, ocupando `0x1000..0x1010`)
- `succeeded() = true`

Linhas localizadas (`result.lines`):

| Linha | Fonte | Categoria | Formato / Diretiva | Endereço | Tamanho |
| --- | --- | --- | --- | --- | --- |
| 1 | `. Exemplo SIC/XE de passagem 1` | `Comment` | — | `0x000000` (`nullopt` na listagem) | `0` |
| 2 | `PROG   START  0x1000` | `Directive` | `Start` | `0x001000` | `0` |
| 3 | `FIRST  CLEAR  X` | `Instruction` | `Format2` (`0xB4`) | `0x001000` | `2` |
| 4 | `       LDB    #CALLEE` | `Instruction` | `Format3` (`0x68`) | `0x001002` | `3` |
| 5 | `       BASE   CALLEE` | `Directive` | `Base` | `0x001005` (`nullopt` na listagem) | `0` |
| 6 | `LOOP   LDA    #5` | `Instruction` | `Format3` (`0x00`) | `0x001005` | `3` |
| 7 | `       +JSUB  CALLEE` | `Instruction` | `Format4` (`0x48`, `+`) | `0x001008` | `4` |
| 8 | `       ADDR   A,X` | `Instruction` | `Format2` (`0x90`) | `0x00100C` | `2` |
| 9 | `       NOBASE` | `Directive` | `NoBase` | `0x00100E` (`nullopt` na listagem) | `0` |
| 10 | `CALLEE RSUB` | `Instruction` | `Format3` (`0x4C`) | `0x00100E` | `3` |
| 11 | `       END    FIRST` | `Directive` | `End` | `0x001011` (`nullopt` na listagem) | `0` |

Tabela de símbolos resultante (`result.symbols`):

| Símbolo | Endereço | Posição de definição |
| --- | --- | --- |
| `PROG` | `0x001000` | linha 2, coluna 1 |
| `FIRST` | `0x001000` | linha 3, coluna 1 |
| `LOOP` | `0x001005` | linha 6, coluna 1 |
| `CALLEE` | `0x00100E` | linha 10, coluna 1 |

## Regras do contador, rótulos e diretivas

- **Contador inicial e `START`**: o contador começa em `0`. Apenas a primeira
  linha ativa pode ser `START endereço`, fixando o contador inicial e o
  `startAddress` em `0x00000..0xFFFFF`. Se `START` possuir rótulo, ele recebe o
  endereço definido por `START`. Sem `START`, `startAddress` e `initialPc`
  padrão permanecem `0`.
- **Tamanhos de instruções**: as 10 instruções de formato 2 ocupam `2` bytes e
  rejeitam `+`; as 27 instruções de memória ocupam `3` bytes no formato 3 e `4`
  bytes com `+` (formato 4); `RSUB` ocupa `3` bytes, não aceita operando e
  rejeita `+`.
- **Rótulos e referências futuras**: o rótulo de uma instrução recebe o
  contador **antes** do avanço. Referências futuras em instruções, `BASE` ou
  `END` não alteram o formato nem o tamanho fixado na passagem 1, e são
  verificadas ao final da passagem.
- **`BASE` e `NOBASE`**: não ocupam bytes (`size = 0`) e permanecem na ordem
  física do intermediário para que a passagem 2 controle a janela do
  registrador base.
- **Limites de memória sem wrap**: os cálculos usam aritmética larga de 64 bits
  (`std::uint64_t`). Cada byte alocado deve pertencer a `0x00000..0xFFFFF`
  (`locationCounter <= 0xFFFFF` e `locationCounter + size <= 0x100000`). Uma
  instrução cujo último byte é `0xFFFFF` é válida; qualquer tentativa de iniciar
  em `0x100000` ou ultrapassar `0xFFFFF` emite `AddressOutOfRange` e interrompe
  o avanço do contador, sem wrap.
- **`END` obrigatório**: encerra as declarações ativas e aceita rótulo de
  entrada opcional definido na tabela de símbolos. Declarações ativas após
  `END` produzem erro; comentários e linhas em branco após `END` permanecem
  permitidos.
- **Diretivas de dados antes do pacote 5**: `BYTE`, `WORD`, `RESB` e `RESW`
  são classificadas em `Data`/`Reservation` e rejeitadas explicitamente com
  `DataDirectiveNotIntegrated`, sem inventar tamanho nem avançar o contador até
  a integração do pacote 5.

## Diagnósticos estruturados

| Código (`PassOneDiagnosticCode`) | Posição apontada | Significado |
| --- | --- | --- |
| `InvalidSourceLine` | Posição do erro léxico/sintático ou início da linha `Invalid` | Entrada rejeitada pelo `SourceParser` |
| `InvalidLabel` | Posição do rótulo | Rótulo inválido ou colisão com mnemônico/diretiva reservada |
| `DuplicateSymbol` | Posição da redefinição (`previousDefinition` guarda a 1ª definição) | Rótulo definido mais de uma vez |
| `UndefinedSymbol` | Posição do símbolo no operando | Símbolo referenciado em instrução, `BASE` ou `END` nunca definido |
| `DuplicateStart` | Posição da operação `START` | Mais de uma diretiva `START` na fonte |
| `MisplacedStart` | Posição da operação `START` | `START` após outra declaração ativa |
| `InvalidStartOperand` | Posição do operando (ou da operação se ausente) | Endereço inicial ausente, negativo, simbólico ou fora de `0x00000..0xFFFFF` |
| `DuplicateEnd` | Posição da operação `END` | Mais de uma diretiva `END` |
| `MissingEnd` | Última linha física (coluna 1) | Fonte terminou sem diretiva `END` |
| `StatementAfterEnd` | Posição da operação após `END` | Declaração ativa depois de `END` |
| `InvalidEndOperand` | Posição do operando de `END` | Operando de `END` não é um rótulo válido ou está fora da memória |
| `InvalidDirectiveOperand` | Posição do operando/operação em `BASE`/`NOBASE` | `BASE` sem operando/com valor inválido ou `NOBASE` com operando |
| `UnknownOperation` | Posição da operação | Mnemônico ou diretiva fora do recorte de 38 instruções + 8 diretivas |
| `ExtendedFormatNotAllowed` | Posição da operação com `+` | Prefixo `+` em diretiva, formato 2 ou `RSUB` |
| `InvalidInstructionOperand` | Posição do operando (ou operação se ausente) | Operando ausente, inesperado em `RSUB` ou inválido para o formato |
| `DataDirectiveNotIntegrated` | Posição da diretiva `BYTE`/`WORD`/`RESB`/`RESW` | Diretiva de dados/reserva antes da integração do pacote 5 |
| `AddressOutOfRange` | Posição da operação ou rótulo | Endereço inicial da linha em `0x100000` ou último byte além de `0xFFFFF` |

## Verificação

`tests/assembler/PassOneTest.cpp` exercita pela interface pública:

- sequência de endereços e rótulos com formatos 2, 3, 4 e `RSUB`;
- referências futuras preservando tamanhos e layout;
- `START` presente/ausente, rótulo inicial, `END` com e sem rótulo de entrada;
- `BASE`, `NOBASE`, linhas em branco e comentários sem avanço do contador;
- duplicação e indefinição de símbolos, colisão de rótulo com mnemônico,
  operação desconhecida, `+` inválido, `END` ausente, segundo `START`/`END` e
  declaração ativa após `END`;
- último endereço válido (`0xFFFFF`), próximo endereço inválido (`0x100000`) e
  overflow sem wrap;
- rejeição explícita de `BYTE`, `WORD`, `RESB` e `RESW` antes do pacote 5;
- reconhecimento das 38 instruções da VM e independência entre invocações.

Para executar os testes do pacote 4 após o build:

```bash
ctest --test-dir build -R 'PassOneTest' --output-on-failure
```

