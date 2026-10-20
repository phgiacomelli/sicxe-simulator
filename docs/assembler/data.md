# Dados e reservas SIC/XE

O pacote 5 está implementado em `src/assembler/DataDirective.*`, no namespace
`assembler`. Consome `SourceLine` do parser, valores e símbolos do pacote 3 e
`LocatedLine` do pacote 4. A biblioteca `sicxe_assembler` continua C++20, sem
Qt ou dependência da implementação da VM.

## Sintaxe e bytes

| Diretiva | Tamanho | Representação codificada |
| --- | --- | --- |
| `BYTE C'Ab ;'` | 4 | `41 62 20 3B` |
| `BYTE X'00aBFF'` | 3 | `00 AB FF` |
| `WORD 0x123456` | 3 | `12 34 56` |
| `WORD -1` | 3 | `FF FF FF` |
| `WORD LABEL` | 3 | Endereço fixo de `LABEL`, big-endian |
| `RESB 5` | 5 | `DataReservation{5}`, sem bytes |
| `RESW 2` | 6 | `DataReservation{6}`, sem bytes |

`BYTE` exige exatamente `C'...'` ou `X'...'`; o prefixo também aceita
minúscula. Caracteres preservam caixa, espaços e pontuação. `C` aceita os
128 valores ASCII (`00..7F`), sem conversão de encoding ou escapes. UTF-8
com bytes acima de `7F` é rejeitado. `X` aceita `0..9`, `a..f` e `A..F`, em
quantidade par, sem espaços ou prefixo `0x` dentro das aspas. Aspas internas
não são escapadas. `C''` e `X''` são dados de tamanho zero.

`WORD` reutiliza a leitura decimal/hexadecimal, sinais e nomes sem distinção
de caixa de [OperandValue](symbols.md). A faixa é `-0x800000..0xFFFFFF`:
negativos usam complemento de dois nos 24 bits inferiores. Não há truncamento
de valores fora da faixa. Rótulos anteriores, futuros e autorreferências
representam endereços fixos, sem relocação ou expressões.

`RESB` e `RESW` exigem inteiro não negativo conhecido na primeira passagem.
Mesmo rótulos já definidos são rejeitados como quantidade, conforme o contrato
de `n` inteiro. Zero é permitido; o sinal negativo, inclusive `-0`, é rejeitado
pelo validador do pacote 3. A multiplicação por três só ocorre depois da
verificação do limite. Nenhuma diretiva pode ocupar mais de `AddressSpaceBytes`.

## Interfaces

```cpp
DataMeasureResult measureData(const SourceLine& line, SymbolTable& symbols);
DataEncodeResult encodeData(const LocatedLine& line, const SymbolTable& symbols);
```

`DataMeasureResult` é `variant<DataMeasurement, DataDiagnostic>`.
`DataMeasurement` contém `sizeBytes` e `reservation`. A medição valida o
operando sem construir bytes. Para `WORD`, registra a referência em
`SymbolTable` via `referenceOperand`; não resolve um rótulo futuro nem inventa
um valor provisório. Cada linha deve ser medida uma vez por montagem, na
passagem 1. Inteiros são validados com `Word24BitRange`; quantidades usam
`parseKnownInteger` e `NonNegativeIntegerRange`.

`DataEncodeResult` é `variant<EncodedData, DataDiagnostic>` e `EncodedData` é
`variant<DataBytes, DataReservation>`. `DataBytes` possui `bytes`;
`DataReservation` possui somente `sizeBytes`. A variante distingue uma reserva
de zero bytes de um literal vazio. A codificação não altera símbolos: usa
`resolveOperand` para consultar o endereço final de `WORD`.

Um único planejamento privado interpreta o operando estruturado nas duas
operações. Não se reabre a fonte nem se chama `SourceParser` novamente.
Assim, medição e emissão compartilham a validação dos literais e os tamanhos.
Não foi acrescentado cache ou campo público ao intermediário dos outros pacotes.

Antes de codificar, `encodeData` confere tamanho, categoria, diretiva e ausência
de formato estendido contra `LocatedLine`, além do limite endereço + tamanho.
Um layout divergente falha em vez de alterar o contador ou emitir bytes parciais.
Reservas nunca alocam um vetor de zeros. Somente o writer `.bin` do pacote 7
preenche o espaço ao construir a imagem final em memória.

## Integração com a passagem 1

`PassOne::run` chama `measureData` para as quatro diretivas e conserva o mesmo
algoritmo de avanço e validação em 64 bits usado nas instruções. O rótulo recebe
o endereço anterior ao avanço, somente depois de validar o tamanho e a extensão.
`BYTE`/`WORD` recebem categoria `Data`; `RESB`/`RESW`, `Reservation`. Não há
opcode, formato de instrução ou bytes em nenhuma dessas linhas.

O intervalo continua `0x00000..0xFFFFF`; uma região pode terminar em
`0x100000`, mas nenhuma nova linha de dados/reserva pode começar nesse endereço,
mesmo que tenha tamanho zero. `END` sem rótulo continua aceito após o último
byte. Literais vazios e reservas zero dentro da faixa conservam o contador.

Referências de `WORD` participam da checagem de indefinidos ao fim da passagem 1.
Falha de medição ou extensão invalida o layout posterior, sem definir o rótulo
da linha inválida nem dar endereço fictício às próximas linhas. A passagem 2
só deve consumir `PassOneResult` quando `succeeded()` for verdadeiro.

## Diagnósticos

`DataDiagnostic` contém `code`, posição física, mensagem e `operandDiagnostic`
opcional. Este último preserva código, token e posição dos erros do pacote 3.
`toAssemblyDiagnostic` usa a conversão desse pacote quando presente.

| Código de dados | Causa / posição | Conversão compartilhada |
| --- | --- | --- |
| `InvalidSourceLine` | Linha não sintaticamente válida; coluna 1 | `InvalidOperation` |
| `InvalidDirective` | Operação diferente das quatro diretivas; operação | `InvalidOperation` |
| `MissingOperand` | Operando ausente; operação | `EmptyOperand` |
| `InvalidByteLiteral` | Prefixo/aspas inválidos; operando ou aspa interna | `UnexpectedField` |
| `NonAsciiCharacter` | Primeiro byte acima de `7F` | `ValueOutOfRange` |
| `InvalidHexDigit` | Primeiro dígito inválido | `UnexpectedField` |
| `OddHexLength` | Quantidade ímpar; início do operando | `UnexpectedField` |
| `InvalidOperand` | Erro numérico/simbólico do pacote 3 | Conversão original do erro |
| `SizeOutOfRange` | Reserva, dados, endereço ou valor simbólico de WORD fora da faixa | `ValueOutOfRange` |
| `LayoutMismatch` | Tamanho/categoria/diretiva divergente; operação | `UnexpectedField` |

Colunas contam bytes e começam em 1, como no parser. Em `BYTE`, caracteres
inválidos são checados antes da paridade hexadecimal. Falhas de número e símbolo
incluem `IntegerOverflow`, `ValueOutOfRange`, `NegativeValueNotAllowed`,
`UnsupportedExpression`, `SymbolNotAllowed` e `UndefinedSymbol` já existentes.

Sem modificar os contratos públicos anteriores, `PassOne` usa seu código
`InvalidDirectiveOperand` para falhas de medição, preservando posição e mensagem;
um avanço inválido usa `AddressOutOfRange`, e indefinidos usam `UndefinedSymbol`.
Quem consome a API de dados diretamente recebe o diagnóstico específico completo.
O enum legado `DataDirectiveNotIntegrated` foi preservado por compatibilidade,
mas não é mais emitido para essas quatro diretivas.

## Verificação e limites do pacote

`DataDirectiveTest` cobre bytes exatos, ASCII e hexadecimal inválidos, strings
vazias, limites signed/unsigned de WORD, símbolos e indefinição, reservas sem
bytes, overflow, divergência de layout e integração com endereços da passagem 1.
`PassOneTest` verifica as quatro diretivas onde antes exigia sua rejeição.
Comandos de verificação permanecem em [CONTRIBUTING](../../CONTRIBUTING.md).

O módulo não codifica instruções nem implementa a passagem 2 completa, writer
ou CLI: esses consumidores estão no pacote 7, descrito em [uso](usage.md).
Relocação e avaliação de expressões permanecem fora do escopo do projeto.
