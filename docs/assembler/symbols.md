# Tabela de símbolos e valores de operandos

O pacote 3 está implementado em `src/assembler/`, no namespace `assembler`.
`SymbolTable` armazena rótulos **locais**, registra referências anteriores e
futuras na ordem física de uso e expõe a tabela final determinística em
`AssemblySymbol`. `OperandValue` classifica um token como inteiro (decimal ou
`0x` hexadecimal) ou rótulo, aplica faixas e restrições de sinal definidas pelo
consumidor e rejeita expressões aritméticas, parênteses e atributos.

## Interface pública

```cpp
#include "assembler/OperandValue.hpp"
#include "assembler/SymbolTable.hpp"

assembler::SymbolTable symbols;
const auto duplicate = symbols.define("loop", 0x1000, {1, 1});
const auto refError = symbols.reference("Target", {2, 10});
const auto value = assembler::parseValue("0x1000", {1, 12});
```

- `SymbolTable::define(label, address, position)` (ou sobrecarga com
  `SourceIdentifier`) insere o rótulo uma única vez com seu endereço em
  **bytes** e a `SourcePosition` original; tentativa de redefinição preserva a
  primeira entrada e retorna `SymbolDiagnostic` com a posição da redefinição em
  `position` e a definição original em `previousDefinition`. A grafia da
  primeira definição também é preservada para a mensagem desse erro.
- `SymbolTable::reference(label, position)` registra o uso do rótulo em ordem
  física; referências futuras permanecem pendentes sem receber endereço falso
  provisório.
- `SymbolTable::resolve(label, position)` devolve o endereço `std::uint32_t`
  definido ou `SymbolDiagnostic` com `UndefinedSymbol` apontando para a posição
  do uso.
- `SymbolTable::symbols()` expõe `const std::vector<AssemblySymbol>&` na ordem
  determinística de definição, sem expor mapa mutável.
- `SymbolTable::unresolvedDiagnostics()` devolve diagnósticos `UndefinedSymbol`
  na ordem física para todas as referências registradas que continuaram sem
  definição ao final da passagem 1.
- `parseValue(token, position)` (ou sobrecarga com `SourceField`) devolve
  `OperandParseResult` (`std::variant<OperandValue, OperandDiagnostic>`), onde
  `OperandValue` distingue `IntegerOperand` de `SymbolOperand`.
- `validateInteger`, `parseKnownInteger`, `referenceOperand` e
  `resolveOperand` permitem ao consumidor exigir inteiro já conhecido na
  passagem 1 (`START`, `RESB`, `RESW`) ou aceitar rótulo anterior/futuro com
  faixa numérica específica (`WORD`, `BASE`, `END` e instruções).
- `toAssemblyDiagnostic(SymbolDiagnostic)` e
  `toAssemblyDiagnostic(OperandDiagnostic)` convertem erros desses módulos
  para o contrato final `AssemblyDiagnostic`, mantendo código tipado,
  posição e mensagem. O consumidor reúne esses erros em `AssemblyFailure`;
  nenhuma conversão produz bytes ou listagem parciais.

## Regras de nomes e palavras reservadas

- Rótulos seguem `[A-Za-z][A-Za-z0-9_]*` em ASCII.
- A comparação não distingue maiúsculas e minúsculas: `SymbolTable::normalizeName`
  converte letras ASCII para maiúsculas na chave (`AssemblySymbol::name`,
  `SymbolReference::name` e `SymbolOperand::name`), preservando a grafia e a
  `SourcePosition` originais para diagnósticos.
- Rótulos não podem colidir com os **38 mnemônicos** reconhecidos pela VM
  (`ADD`, `ADDR`, `AND`, `CLEAR`, `COMP`, `COMPR`, `DIV`, `DIVR`, `J`, `JEQ`,
  `JGT`, `JLT`, `JSUB`, `LDA`, `LDB`, `LDCH`, `LDL`, `LDS`, `LDT`, `LDX`,
  `MUL`, `MULR`, `OR`, `RMO`, `RSUB`, `SHIFTL`, `SHIFTR`, `STA`, `STB`,
  `STCH`, `STL`, `STS`, `STT`, `STX`, `SUB`, `SUBR`, `TIX`, `TIXR`) nem com as
  **8 diretivas** do dialeto (`START`, `END`, `BYTE`, `WORD`, `RESB`, `RESW`,
  `BASE`, `NOBASE`).
- Identificadores de uma letra que não são mnemônicos nem diretivas (como `A`,
  `B` e `X`) são aceitos como nomes válidos de rótulo quando usados em posição
  simbólica.

## Valores numéricos e restrições por consumidor

- Inteiros aceitam notação decimal (`[+-]?[0-9]+`) ou hexadecimal com prefixo
  `0x`/`0X` (`[+-]?0[xX][0-9A-Fa-f]+`).
- A leitura numérica acumula magnitude sem comportamento indefinido em
  `std::int64_t`, preservando a base (`Decimal` ou `Hexadecimal`), a presença
  de sinal negativo (`negative`), o texto original e a `SourcePosition`.
- Não há avaliador de expressões: tokens compostos com operadores ou
  parênteses (`A+1`, `A-B`, `2*B`, `(A)`, `L(BUFFER)`, `+A`, `-B`) são
  rejeitados com `UnsupportedExpression`.
- O consumidor especifica `IntegerRange {min, max, allowNegative}`. Constantes
  pré-definidas cobrem as faixas arquiteturais recorrentes:
  - `Address12BitRange`: `0..0xFFF`, sem sinal negativo;
  - `Address20BitRange`: `0..0xFFFFF`, sem sinal negativo (`START`, `BASE`,
    formato 4);
  - `Word24BitRange`: `-0x800000..0xFFFFFF`, com sinal (`WORD`);
  - `NonNegativeIntegerRange`: `0..INT64_MAX`, sem sinal negativo (`RESB`,
    `RESW`).

## Diagnósticos estruturados

### `SymbolDiagnosticCode` (`SymbolTable`)

| Código | Posição apontada | `previousDefinition` |
| --- | --- | --- |
| `InvalidSymbolName` | Posição da definição, referência ou resolução | `std::nullopt` |
| `ReservedSymbolName` | Posição do rótulo que colide com mnemônico/diretiva | `std::nullopt` |
| `DuplicateSymbol` | Posição da tentativa de redefinição | Posição da primeira definição |
| `UndefinedSymbol` | Posição do uso/referência do símbolo | `std::nullopt` |

### `OperandDiagnosticCode` (`OperandValue`)

| Código | Significado |
| --- | --- |
| `EmptyOperand` | Token vazio |
| `UnsupportedExpression` | Operador aritmético, sinal aplicado a símbolo, parênteses ou atributo |
| `InvalidInteger` | Literal numérico sem dígitos ou com caractere inválido para a base |
| `IntegerOverflow` | Literal inteiro excede o intervalo representável de 64 bits |
| `NegativeValueNotAllowed` | Valor negativo (inclusive `-0`) em operando que exige inteiro não negativo |
| `ValueOutOfRange` | Inteiro fora do intervalo `[min, max]` exigido pelo consumidor |
| `InvalidSymbolName` | Token simbólico fora do padrão `[A-Za-z][A-Za-z0-9_]*` |
| `ReservedSymbolName` | Token simbólico colide com mnemônico ou diretiva |
| `DuplicateSymbol` | Redefinição repassada por um consumidor de símbolos |
| `SymbolNotAllowed` | Símbolo fornecido onde o consumidor exige inteiro conhecido na passagem 1 |
| `UndefinedSymbol` | Símbolo resolvido na passagem 2 sem definição na `SymbolTable` |

`AssemblyDiagnosticCode` possui os códigos correspondentes acima para que
falhas de símbolo e operando possam integrar `AssemblyFailure`. Os códigos
lexicais do parser continuam documentados em [fonte](source.md).

## Verificação

`SymbolTableTest` cobre definições, referências anteriores e futuras, múltiplas
referências ao mesmo rótulo, normalização de caixa, nomes inválidos, colisão
com mnemônicos/diretivas, duplicação com ambas as posições, símbolos
indefinidos, exportação determinística e isolamento entre montagens.
`OperandValueTest` cobre inteiros decimais e hexadecimais, zero, limites de 64
bits, overflow, faixas de 12/20/24 bits, rejeição de expressões/parênteses e o
fluxo de duas passagens integrado ao `SourceParser`.
