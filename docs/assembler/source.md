# Fonte e contratos mínimos do montador

O pacote 1 está implementado em `src/assembler/`, no namespace `assembler`.
`SourceParser` transforma texto em linhas estruturadas e diagnósticos. Ele
não monta instruções nem determina se um mnemônico pertence às 38 instruções:
essa validação pertence ao catálogo do pacote 2. A biblioteca
`sicxe_assembler` é C++20 e não depende de Qt nem de `SICXECore`.

## Interface pública

```cpp
#include "assembler/SourceParser.hpp"

const auto parsed = assembler::SourceParser::parse("loop lda #Value\n");
```

`parse(std::string_view)` retorna `SourceParseResult`, com `lines`,
`diagnostics` e `succeeded()`. O resultado possui suas próprias strings;
o chamador pode destruir ou modificar o texto original. Não há I/O,
estado compartilhado ou exigência de terminar o texto com nova linha.

`succeeded()` significa apenas ausência de **erro de sintaxe nesta etapa**.
Os próximos pacotes ainda precisarão validar opcodes, registradores,
operandos, diretivas, faixas e símbolos. Se houver diagnóstico, as futuras
passagens devem interromper a montagem; linhas válidas retidas no resultado
servem para inspeção, não autorizam uma saída parcial.

## Representação das linhas

Cada `SourceLine` possui:

| Campo | Conteúdo |
| --- | --- |
| `number` | Linha física, começando em 1 |
| `text` | Texto original sem o terminador de linha |
| `kind` | `Blank`, `Comment`, `Statement` ou `Invalid` |
| `statement` | `ParsedStatement` presente somente em `Statement` |
| `comment` | Campo opcional que inclui o marcador `.` ou `;` |

`ParsedStatement` contém `label` opcional, `operation` e `operand` opcional.
Rótulo e operação usam `SourceIdentifier`: `source` preserva texto/posição e
`key` contém a chave ASCII em maiúsculas. Um prefixo `+` permanece na chave.
Operando e comentário usam `SourceField`, com texto original e
`SourcePosition {line, column}`; não há conversão de caixa nesses campos.

No exemplo acima, a chave do rótulo é `LOOP`, a operação é `LDA`, e o
operando continua `#Value`. As colunas são respectivamente 1, 6 e 10.
Colunas contam **bytes**, iniciadas em 1; tab conta como um byte, não como
uma quantidade visual de espaços. Isso permite apontar o texto original
sem depender de configuração de editor ou locale.

## Regras lexicais

- Fonte aceita terminadores LF, CRLF e CR; CRLF conta como um único salto.
  Fonte vazia tem zero linhas. Um terminador final não cria uma linha
  adicional; duas quebras consecutivas preservam a linha vazia entre elas.
- Espaço e tab separam campos. Rótulo começa na coluna 1; instrução sem
  rótulo precisa de espaço/tab inicial. `RSUB` sozinho na coluna 1 é lido
  como rótulo sem operação e recebe `MissingOperation`; `  RSUB` é válido
  para o parser. Não se consulta um catálogo para adivinhar o significado.
- Rótulos seguem `[A-Za-z][A-Za-z0-9_]*`. A operação segue a mesma forma,
  com `+` opcional. Palavras desconhecidas são preservadas para validação
  semântica posterior; colisão de rótulo com operação também é posterior.
- `.` na primeira posição não branca e `;` sem declaração iniciam
  comentários integrais. `;` fora de aspas inicia comentário inline,
  inclusive sem espaço antes. Aspas em comentários não são analisadas.
- Aspas simples protegem espaços, vírgulas e `;` no operando e precisam
  fechar na mesma linha. O parser não interpreta o valor de `C'...'` ou
  `X'...'`, não valida seus dígitos e não implementa escapes de aspas.
- O operando conserva prefixos `#`/`@`, vírgulas e caixa. Espaços nas
  extremidades são removidos; dentro de aspas ou junto a vírgulas são
  preservados (`A , X`). Outro campo separado por branco fora de aspas,
  como `#1 extra`, recebe `UnexpectedField`. Quantidade de operandos,
  vírgulas e modos permitidos será validada pelo pacote 2.

## Diagnósticos e recuperação

`AssemblyDiagnostic` contém `code`, `position` e `message`. Mensagens são
apresentação humana; consumidores devem usar o enum e a posição. Os códigos
abaixo são os que o parser emite; [símbolos](symbols.md) documenta os códigos
semânticos acrescentados ao mesmo enum pelo pacote 3.

| Código | Posição apontada |
| --- | --- |
| `InvalidLabel` | Primeiro caractere inválido do rótulo |
| `MissingOperation` | Fim do trecho de código + 1, antes do comentário, se houver |
| `InvalidOperation` | Primeiro caractere inválido da operação; `+` se estiver sozinho |
| `UnterminatedQuote` | Aspas de abertura que ficaram sem fechamento |
| `UnexpectedField` | Primeiro caractere do campo adicional |

Há no máximo um diagnóstico por linha, nessa ordem de precedência. Após
um erro, a linha recebe `Invalid` e não contém `statement` parcial; texto
e comentário reconhecido ficam disponíveis. O parser continua na próxima
linha, preservando a ordem física dos diagnósticos. Ele não verifica
presença de `START`/`END`, símbolos ou quantidade de instruções.

## Contrato do resultado da montagem

`AssemblyResult.hpp` materializa os valores usados pelo pacote 7:

- `AssemblySuccess`: `loadAddress`, `initialPc`, imagem `bytes`, `listing`
  e `symbols`;
- `AssemblyListingLine`: `line`, `source`, endereço opcional, bytes da
  linha e `reservedBytes` (zero nas linhas que não são reserva);
- `AssemblySymbol`: nome normalizado, endereço e posição da definição;
- `AssemblyFailure`: apenas `diagnostics`.

`AssemblyResult` é `std::variant<AssemblySuccess, AssemblyFailure>`:
uma falha não oferece imagem ou listagem de sucesso parcial. Os valores
são contratos de dados, não responsabilidades do parser. A implementação
atual de `assemble()`/`PassTwo` e `RawImageWriter`, do pacote 7, garante
imagem não vazia, faixas válidas, bytes e reservas mutuamente exclusivos
na mesma linha e endereços coerentes com o layout; veja [uso](usage.md).

## Verificação

`SourceParserTest` cobre 22 casos de fonte, posições, preservação de dados,
erros e independência entre chamadas. `AssemblyContractTest` tem dois
casos de uso dos tipos compartilhados, com imagem/listagem/símbolos e
falha tipada. Ambos rodam no alvo `AssemblerTests`, inclusive sem Qt.
Os comandos estão em [testes](../testing.md#montador-pacote-1).

O contrato comportamental do dialeto permanece em
[contracts.md](contracts.md); este arquivo documenta os tipos e a
implementação efetivamente entregues.
