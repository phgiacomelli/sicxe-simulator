# Passagem 1 e contador de localização

A passagem 1 do montador SIC/XE está implementada em `src/assembler/LocatedLine.hpp`,
`src/assembler/PassOne.hpp` e `src/assembler/PassOne.cpp`, no namespace `assembler`.
Ela consome as linhas já analisadas sintaticamente pelo `SourceParser` (pacote 1),
consulta o catálogo e a validação de operandos da ISA (`InstructionCatalog` e
`InstructionOperand`, pacote 2) e registra definições e referências de rótulos na
`SymbolTable` com validação de inteiros/símbolos via `OperandValue` (pacote 3).

## Responsabilidade e fronteira

- Percorre a sequência de `SourceLine` **uma única vez**, mantendo o contador de
  localização (`LOCCTR`) em **bytes** dentro do espaço endereçável da VM
  (`0x00000..0xFFFFF`, capacidade total de `0x100000` bytes).
- Produz uma sequência imutável de `LocatedLine` com categoria, endereço e tamanho
  fixados para consumo da passagem 2, sem reler o arquivo-fonte e
  sem permitir reposicionamento ou promoção posterior de formato.
- Consome `SourceLine` somente quando `SourceParseResult::succeeded()` é verdadeiro.
  Se o parser sintático reportou falha ou se qualquer linha `Invalid` for entregue
  diretamente a `PassOne::run`, a passagem 1 recusa a montagem parcial e reporta
  `PassOneDiagnosticCode::InvalidSourceLine`.

## Contrato do intermediário (`LocatedLine` e `PassOneResult`)

Cada `LocatedLine` preserva a `SourceLine` original e fixa:

- `category` (`LocatedLineCategory`): `Instruction`, `Data`, `Reservation`,
  `Directive` ou `Comment`;
- `address` (`std::uint32_t`): endereço em bytes atribuído antes do avanço do
  contador;
- `sizeBytes` (`std::size_t`): número de bytes emitidos ou reservados (`2`, `3` ou
  `4` para instruções; `0` para `START`, `END`, `BASE`, `NOBASE` e comentários);
- `mnemonic` (`std::string`): mnemônico ou diretiva normalizada sem o prefixo `+`;
- `extended` (`bool`): indica se a instrução usou o prefixo `+` (formato 4);
- `opcode` (`std::optional<std::uint8_t>`): opcode da instrução quando
  `category == LocatedLineCategory::Instruction`;
- `format` (`std::optional<LocatedInstructionFormat>`): `Format2` (`2`), `Format3`
  (`3`) ou `Format4` (`4`);
- `directive` (`std::optional<LocatedDirectiveKind>`): `Start`, `End`, `Base`,
  `NoBase`, `Byte`, `Word`, `Resb` ou `Resw`.

`LocatedLine` conserva os campos estruturados em `source.statement`, mas não
armazena um `InstructionOperand` separado. Os consumidores dos pacotes 6/7
podem reutilizar `parseInstructionOperand` sobre essa `SourceLine` para obter
registradores e modos tipados, preservando endereço e tamanho já fixados.

`PassOneResult` reúne:

- `startAddress`: endereço inicial definido por `START` (ou `0` quando omitido);
- `initialPc`: endereço de entrada resolvido pelo operando opcional de `END` (ou
  igual a `startAddress` quando `END` não tem operando);
- `programLength`: extensão total em bytes (`LOCCTR_final - startAddress`);
- `lines`: `std::vector<LocatedLine>` na mesma ordem física da entrada;
- `symbolTable`: instância de `SymbolTable` com todas as definições e referências
  registradas na ordem física, pronta para resolução na passagem 2;
- `symbols`: `std::vector<AssemblySymbol>` exportado em ordem determinística de
  definição;
- `diagnostics`: `std::vector<PassOneDiagnostic>` com código tipado, `SourcePosition`
  e `previousDefinition` opcional; `toAssemblyDiagnostic` converte qualquer falha da
  passagem 1 para o contrato compartilhado `AssemblyDiagnostic`.

## Exemplo de intermediário SIC/XE real

Para a fonte SIC/XE abaixo:

```asm
       START 0x1000
FIRST  CLEAR A
STEP1  LDA   #5
       BASE  CALLEE
STEP2  +JSUB CALLEE
       . comentario preservado na ordem
STEP3  ADDR  A,X
       NOBASE
CALLEE RSUB
       END   FIRST
```

`PassOne::run` produz `startAddress = 0x1000`, `initialPc = 0x1000`,
`programLength = 14` (`0x0E` bytes) e a seguinte sequência de `LocatedLine`:

| Linha | Fonte | Categoria | Formato / Diretiva | `address` | `sizeBytes` | `opcode` |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `START 0x1000` | `Directive` | `Start` | `0x01000` | `0` | — |
| 2 | `FIRST  CLEAR A` | `Instruction` | `Format2` | `0x01000` | `2` | `0xB4` |
| 3 | `STEP1  LDA   #5` | `Instruction` | `Format3` | `0x01002` | `3` | `0x00` |
| 4 | `BASE  CALLEE` | `Directive` | `Base` | `0x01005` | `0` | — |
| 5 | `STEP2  +JSUB CALLEE` | `Instruction` | `Format4` (`+`) | `0x01005` | `4` | `0x48` |
| 6 | `. comentario...` | `Comment` | — | `0x01009` | `0` | — |
| 7 | `STEP3  ADDR  A,X` | `Instruction` | `Format2` | `0x01009` | `2` | `0x90` |
| 8 | `NOBASE` | `Directive` | `NoBase` | `0x0100B` | `0` | — |
| 9 | `CALLEE RSUB` | `Instruction` | `Format3` | `0x0100B` | `3` | `0x4C` |
| 10 | `END   FIRST` | `Directive` | `End` | `0x0100E` | `0` | — |

E a tabela de símbolos (`result.symbols`) resultante:

| Símbolo | Endereço (`bytes`) | Posição de definição |
| --- | --- | --- |
| `FIRST` | `0x01000` | linha 2, coluna 1 |
| `STEP1` | `0x01002` | linha 3, coluna 1 |
| `STEP2` | `0x01005` | linha 5, coluna 1 |
| `STEP3` | `0x01009` | linha 7, coluna 1 |
| `CALLEE` | `0x0100B` | linha 9, coluna 1 |

## Regras de endereço, diretivas e limites

- **Contador inicial e `START`**: `LOCCTR` começa em `0`. `START` é aceito apenas na
  primeira linha ativa (comentários e linhas em branco anteriores são permitidos) e
  exige inteiro não negativo em `0x00000..0xFFFFF` (`Address20BitRange`). Se
  `START` possuir rótulo, ele é definido com o endereço inicial.
- **Tamanhos de instruções**: determinados pelo catálogo (`InstructionCatalog`):
  formato 2 ocupa `2` bytes, formato 3 ocupa `3` bytes (inclusive `RSUB`) e formato
  4 (`+` em instrução `Format34`) ocupa `4` bytes. Referências futuras não alteram
  o tamanho escolhido na passagem 1.
- **`BASE` e `NOBASE`**: têm `sizeBytes = 0` e permanecem na posição original do
  intermediário para que a passagem 2 acompanhe o endereço-base assumido pelo
  montador para calcular deslocamentos, na ordem da fonte. `BASE` não escreve
  o registrador B da VM; o programa precisa carregá-lo. `BASE` aceita rótulo
  (registrado em `SymbolTable`) ou endereço de 20 bits; `NOBASE` não aceita operando.
- **`END`**: obrigatório; encerra as declarações ativas e aceita um rótulo de
  entrada opcional (registrado em `SymbolTable` e resolvido ao final da passagem 1
  para fixar `initialPc`). Declarações ativas após `END`, segundo `START` ou segundo
  `END` são erros explícitos.
- **Limites de memória sem wrap**: o avanço de `LOCCTR` usa aritmética de 64 bits
  (`std::uint64_t`) e valida `address <= 0xFFFFF` e `address + sizeBytes <= 0x100000`.
  O byte `0xFFFFF` é aceito; qualquer instrução que inicie em `0x100000` ou
  ultrapasse `0xFFFFF` produz `AddressOutOfRange` e interrompe o cálculo de layout.
- **Diretivas de dados antes do pacote 5**: enquanto o pacote 5 não estiver
  integrado, `BYTE`, `WORD`, `RESB` e `RESW` recebem
  `PassOneDiagnosticCode::DataDirectiveNotIntegrated` e invalidam o layout seguinte,
  sem definir seu rótulo nem publicar uma `LocatedLine` de tamanho desconhecido.
  Os limites de endereço usados pelo layout e por `Address20BitRange` têm
  definição única em `AddressLimits.hpp`.

## Diagnósticos da passagem 1

| `PassOneDiagnosticCode` | Causa | Conversão em `toAssemblyDiagnostic` |
| --- | --- | --- |
| `InvalidSourceLine` | Entrada com erro sintático do `SourceParser` ou linha `Invalid` | `InvalidOperation` |
| `DuplicateSymbol` | Rótulo redefinido (inclui `previousDefinition`) | `DuplicateSymbol` |
| `UndefinedSymbol` | Símbolo referenciado em instrução, `BASE` ou `END` nunca definido | `UndefinedSymbol` |
| `InvalidLabel` | Rótulo inválido ou colisão com mnemônico/diretiva reservada | `InvalidLabel` |
| `UnknownOperation` | Operação fora das 38 instruções e das 8 diretivas (ex.: `ADDF`, `EQU`, `ORG`) | `InvalidOperation` |
| `ExtendedFormatNotAllowed` | Prefixo `+` em formato 2, `RSUB` ou diretiva | `InvalidOperation` |
| `DuplicateStart` | Mais de uma diretiva `START` | `InvalidOperation` |
| `MisplacedStart` | `START` após outra linha ativa | `InvalidOperation` |
| `InvalidStartOperand` | Operando de `START` ausente, simbólico, negativo ou fora de 20 bits | `UnexpectedField` |
| `MissingEnd` | Fim do arquivo sem diretiva `END` | `InvalidOperation` |
| `DuplicateEnd` | Segunda diretiva `END` | `InvalidOperation` |
| `StatementAfterEnd` | Declaração ativa após `END` | `InvalidOperation` |
| `InvalidEndOperand` | Operando de `END` numérico, expressão ou nome reservado | `UnexpectedField` |
| `InvalidDirectiveOperand` | Operando inválido em `BASE` ou operando presente em `NOBASE` | `UnexpectedField` |
| `InvalidInstructionOperand` | Operando de instrução inválido segundo `InstructionOperand` / `OperandValue` | `UnexpectedField` |
| `DataDirectiveNotIntegrated` | `BYTE`, `WORD`, `RESB` ou `RESW` antes da integração do pacote 5 | `InvalidOperation` |
| `AddressOutOfRange` | `LOCCTR` ou instrução excede `0x00000..0xFFFFF` | `ValueOutOfRange` |
