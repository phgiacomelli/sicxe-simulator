# Catálogo da ISA, operandos e formato 2

Implementação do pacote 2 no namespace `assembler`, em `InstructionCatalog.*`,
`InstructionOperand.*` e `Format2Encoder.*`. A biblioteca `sicxe_assembler`
continua independente de Qt e da implementação da VM. Somente o alvo de testes
cruzados `AssemblerDecoderTests` liga o montador ao núcleo da VM.

A referência disponível foi o [contrato do grupo](contracts.md), confrontado
com `src/core/Instruction.hpp`, `src/core/Decoder.cpp` e a
[documentação do decoder](../decoder.md). Os PDFs distribuídos fora do Git não
estavam disponíveis e não foram consultados.

## Catálogo público e imutável

`instructionCatalog()` retorna `std::span<const InstructionSpec>` sobre uma
única tabela `constexpr` de 38 entradas. `findInstruction(mnemonic, position)`
ou a sobrecarga com `SourceIdentifier` retorna `InstructionLookupResult`:
`std::reference_wrapper<const InstructionSpec>` ou `InstructionDiagnostic`.
A referência permanece válida por toda a execução. A consulta não altera
estado e compara ASCII sem distinção de caixa.

O descritor contém `mnemonic`, `opcode`, `formats` e `operands`. `size()`
retorna o tamanho padrão em bytes; `supportsExtended()` informa se `+` é
permitido. A consulta também aceita `+` e valida sua legalidade, mas não cria
um descritor adicional. O formato escolhido e o tamanho efetivo ficam no
resultado de `parseInstructionOperand`, inclusive para o consumidor da
passagem 1. Diretivas não são instruções e falham na consulta.

### Formato 2 — dez instruções, dois bytes

| Mnemônico | Opcode hexadecimal | Padrão do operando |
| --- | --- | --- |
| ADDR | 90 | r1,r2 |
| CLEAR | B4 | r1 |
| COMPR | A0 | r1,r2 |
| DIVR | 9C | r1,r2 |
| MULR | 98 | r1,r2 |
| RMO | AC | r1,r2 |
| SHIFTL | A4 | r1,contagem |
| SHIFTR | A8 | r1,contagem |
| SUBR | 94 | r1,r2 |
| TIXR | B8 | r1 |

Os registradores aceitos são `A=0`, `X=1`, `L=2`, `B=3`, `S=4`, `T=5`,
`PC=8` e `SW=9`. `F`, números no lugar dos nomes e códigos desconhecidos
são rejeitados. Caixa é normalizada no enum `InstructionRegister`; cada
`RegisterToken` conserva seu `SourceField` original. Para shifts a contagem
fonte é um inteiro de 1 a 16, decimal ou `0x` hexadecimal, lido pelo
`parseKnownInteger` existente do pacote 3. O resultado conserva tanto a
contagem fonte quanto seu token; somente o encoder subtrai 1.

### Grupo de memória — 28 instruções

| Mnemônico | Opcode hexadecimal | Formatos do montador | Operando |
| --- | --- | --- | --- |
| ADD | 18 | 3/4 | memória |
| AND | 40 | 3/4 | memória |
| COMP | 28 | 3/4 | memória |
| DIV | 24 | 3/4 | memória |
| J | 3C | 3/4 | memória |
| JEQ | 30 | 3/4 | memória |
| JGT | 34 | 3/4 | memória |
| JLT | 38 | 3/4 | memória |
| JSUB | 48 | 3/4 | memória |
| LDA | 00 | 3/4 | memória |
| LDB | 68 | 3/4 | memória |
| LDCH | 50 | 3/4 | memória |
| LDL | 08 | 3/4 | memória |
| LDS | 6C | 3/4 | memória |
| LDT | 74 | 3/4 | memória |
| LDX | 04 | 3/4 | memória |
| MUL | 20 | 3/4 | memória |
| OR | 44 | 3/4 | memória |
| RSUB | 4C | 3 | nenhum |
| STA | 0C | 3/4 | memória |
| STB | 78 | 3/4 | memória |
| STCH | 54 | 3/4 | memória |
| STL | 14 | 3/4 | memória |
| STS | 7C | 3/4 | memória |
| STT | 84 | 3/4 | memória |
| STX | 10 | 3/4 | memória |
| SUB | 1C | 3/4 | memória |
| TIX | 2C | 3/4 | memória |

O padrão é formato 3 (três bytes); `+` solicita formato 4 (quatro bytes)
somente nas 27 entradas `Format34`. `RSUB` pertence ao grupo arquitetural de
memória, mas tem `OperandPattern::None`: aceita apenas formato 3 sem operando.
`+RSUB` e qualquer `+` em formato 2 falham. Formato 1 e sintaxe para SIC
legado não são oferecidos. O pacote 2 não emite bytes dos formatos 3/4,
inclusive de `RSUB`.

## Validação e preservação da fonte

`parseInstructionOperand(spec, parsedLine)` consome uma `SourceLine` após
`SourceParseResult::succeeded()`. O resultado é uma variante entre
`InstructionOperand` e `InstructionDiagnostic`. Rejeita linhas que não sejam
declarações válidas e descritor diferente da operação da linha. A entrada
não é modificada.

`InstructionOperand` possui `form`, `operation` (texto e posição originais),
`size()` e `data`, que distingue ausência, um registrador, dois registradores,
registrador/contagem ou memória. Valores de memória contêm:

- `mode`: `Simple`, `Immediate` (`#`) ou `Indirect` (`@`);
- `indexed`: sufixo `,X`, permitido apenas no modo simples;
- `kind`: token `Integer` ou `Symbol`;
- `value`: `SourceField` próprio com a grafia do valor, sem prefixo/sufixo,
  e sua posição física original de byte.

Assim `+LdA TaRgEt , x` preserva `+LdA` e `TaRgEt`, informa formato 4,
modo simples e indexação. O token `value` pode ser passado diretamente a
`parseValue` do pacote 3. O nome normalizado do símbolo, sua existência,
conversão numérica, overflow e faixas de endereço ficam com os pacotes 3/6.
Não há número/endereço provisório. Até números maiores que 64 bits continuam
como token nesta interface; o pacote 3 reportará overflow ao convertê-los.

A validação lexical aceita inteiro decimal ou `0x` hexadecimal com sinal
opcional e identificadores do dialeto. Rejeita expressão, atributo, literal
`=...`, modos sem valor, vírgulas extras, indexação diferente de `X` e
indexação em imediato/indireto. Imediatos numéricos com sinal negativo,
inclusive `-0`, são rejeitados conforme o contrato da VM. Nenhuma dessas
checagens resolve símbolos, escolhe PC/base ou avança o contador.

## Codificação do formato 2

`encodeFormat2(spec, operand)` retorna `Format2Bytes` (`std::array<uint8_t, 2>`)
ou `InstructionDiagnostic`. É uma função sem estado mutável compartilhado,
sem tabela de símbolos, I/O ou consulta à VM. Valida descritor, formato,
família do operando, códigos de registrador e contagem mesmo quando o chamador
constrói os tipos manualmente. Os valores tipados são a fonte dos bytes;
os tokens preservados servem para diagnóstico.

O primeiro byte é o opcode; o segundo é `r1 << 4 | r2`. Para `CLEAR`/`TIXR`,
o nibble baixo é zero. Para shifts, ele é `contagem - 1`. Exemplos calculados
manualmente: `RMO X,A` → `AC 10`, `CLEAR SW` → `B4 90`, `SHIFTL T,16` →
`A4 5F`. Falhas não oferecem bytes parciais.

## Diagnósticos

`InstructionDiagnostic` contém `InstructionDiagnosticCode`, `source` (token
com posição) e `message`. Testes e consumidores usam código e posição, nunca
o texto humano. Falhas em `+` apontam para a operação; ausência de operando
aponta imediatamente após o mnemônico. Campos vazios apontam para sua posição
de inserção. Tokens e partes delimitadas por vírgula são aparados com ajuste
da coluna, preservando a convenção de byte iniciada em 1 do pacote 1.

| Código do pacote 2 | Posição | Conversão para `AssemblyDiagnosticCode` existente |
| --- | --- | --- |
| UnknownInstruction | operação | InvalidOperation |
| InvalidExtendedFormat | operação, incluindo `+` | InvalidOperation |
| InvalidSourceLine | início da linha | InvalidOperation |
| InstructionMismatch | operação | InvalidOperation |
| MissingOperand | fim da operação + 1 | EmptyOperand |
| UnexpectedOperand | operando de RSUB | UnexpectedField |
| WrongOperandCount | token ou trecho com a vírgula excedente | UnexpectedField |
| InvalidRegister | token do registrador | UnexpectedField |
| InvalidShiftCount | token da contagem | ValueOutOfRange |
| InvalidIndexing | vírgula e sufixo | UnexpectedField |
| MissingValue | posição do valor ausente | EmptyOperand |
| UnsupportedOperandSyntax | token do valor | UnsupportedExpression |
| InvalidValue | token do valor | InvalidInteger |
| NegativeImmediate | token numérico, incluindo sinal | NegativeValueNotAllowed |
| InvalidFormat2Operand | operação | UnexpectedField |

`toAssemblyDiagnostic` adapta para o enum compartilhado existente, mantendo
posição e mensagem. Essa conversão é intencionalmente mais geral: consumidores
que precisam da categoria específica devem consultar `InstructionDiagnostic`
antes de converter. Isso evita alterar os tipos compartilhados já entregues
pelos pacotes 1/3 sem coordenação. Nenhuma assinatura existente foi alterada.

## Integração e validação

A [revisão aprovada do PR #24](https://github.com/phgiacomelli/sicxe-simulator/pull/24#pullrequestreview-5560213589) registra a unificação dos nomes
reservados com o catálogo para o pacote 2. A implementação de `SymbolTable`
e seus testes de integração permanecem pendentes para avaliação/correção pelo
agente do chefe. As oito diretivas devem continuar separadas dos opcodes.
A nova consulta já pode ser consumida sem depender da implementação da VM;
a integração ainda não é considerada concluída.

Os testes em `InstructionCatalogTest`, `InstructionOperandTest`,
`Format2EncoderTest` e `Format2DecoderTest` verificam a tabela exata, tamanhos,
imutabilidade, tokens/posições, sucessos, limites e falhas. Os testes cruzados
cobrem todos os registradores válidos e contagens dos dez opcodes de formato 2;
probes de bytes escritos manualmente verificam os formatos anunciados das 28
entradas de memória. Não há codificador de formato 3/4 neste pacote.
Comandos e resultados locais estão em [testes](../testing.md#montador-pacote-2).
