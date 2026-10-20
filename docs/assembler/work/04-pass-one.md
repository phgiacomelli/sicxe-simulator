# Pacote 4 — Passagem 1 e contador de localização

Estado: implementado e validado; diretivas de dados integradas pelo pacote 5.

## Resultado esperado

Percorrer a fonte **uma vez**, atribuir endereço e tamanho às instruções,
definir rótulos e produzir um intermediário imutável para a passagem 2.
O contador é em bytes, conforme a VM SIC/XE, e nenhum deslocamento pode
mudar o tamanho escolhido depois.

## Dependências e ownership

Depende dos pacotes 1 (linhas), 2 (formatos/tamanhos) e 3 (símbolos).
Arquivos sugeridos: `src/assembler/PassOne.*`,
`src/assembler/LocatedLine.hpp`, `tests/assembler/PassOneTest.cpp`,
`docs/assembler/pass-one.md`. Este PR estabiliza o intermediário; o
pacote 5 integra nele os tamanhos das diretivas de dados com revisão do
responsável por este pacote. A integração está descrita em
[dados e reservas](../data.md); operandos inválidos continuam impedindo
a publicação de linhas de tamanho inventado.

Consumir as `assembler::SourceLine` do pacote 1 somente quando
`SourceParseResult::succeeded()` for verdadeiro. `SourceLine::statement`
contém a declaração válida; branco/comentário conserva sua linha sem
declaração. Não reinterpretar o texto original nem tentar aproveitar
uma linha `Invalid` para montar parcialmente.

## Interface a entregar

`run(parsedLines)` devolve `PassOneResult` com endereço `START`, PC de
entrada, linhas localizadas, extensão, tabela de símbolos e diagnósticos.
Cada linha fixa categoria (instrução, dados, reserva, diretiva, comentário),
endereço e tamanho. O pacote 5 acrescenta o cálculo de dados/reservas, sem
alterar os casos existentes. A passagem 2 não reabre nem reposiciona linhas.

## Comportamento obrigatório

- Contador inicial 0; `START` apenas na primeira linha ativa pode defini-lo.
  Rótulo de `START` recebe esse endereço. Sem `START`, início e PC padrão 0.
- Formato 2 ocupa 2 bytes, formato 3 ocupa 3, `+` formato 4 ocupa 4.
  `RSUB` ocupa 3. `BASE`/`NOBASE` não ocupam bytes e permanecem na ordem
  original do intermediário para a passagem 2.
- Rótulo de instrução/dado recebe o contador **antes** do avanço. Usar
  aritmética larga e validar `0x00000..0xFFFFF`, inclusive último byte.
  Não fazer wrap ou continuar após tamanho inválido.
- `END` é obrigatório, encerra linhas ativas e recebe rótulo de entrada
  opcional; endereço de entrada deve ser válido. Fonte ativa depois de
  `END`, segundo `START`/`END` e opcode desconhecido são erros.
- No fim, verificar símbolos indefinidos; com erro, não iniciar a
  passagem 2. Dados/reservas serão integrados pelo pacote 5 no mesmo
  contador e intermediário.

## Testes obrigatórios

- endereços de labels em sequência com formatos 2/3/4 e `RSUB`;
- referência futura preservada sem mudar layout;
- `START` presente/ausente, rótulo inicial, `END` e PC opcional;
- `BASE`/`NOBASE` mantidos sem incremento;
- erros de duplicação/indefinição, instrução desconhecida, `END` ausente
  ou texto posterior;
- último endereço válido, próximo endereço inválido e overflow;
- integração das quatro diretivas de dados e reservas com tamanhos validados
  pelo pacote 5 (substitui a rejeição anterior à integração).

## Fora do escopo

Não calcular PC/base, codificar opcodes, emitir binário ou implementar
`EQU`, `ORG`, `LTORG`, pools, múltiplas seções ou várias localizações.
Não carregar bytes na `Machine`.

## Critério de conclusão

- [x] Cada instrução tem endereço/tamanho estáveis para a segunda passagem.
- [x] `START`, `END`, labels, limites e erros possuem testes.
- [x] `docs/assembler/pass-one.md` mostra um intermediário SIC/XE real.
- [x] Build, formatação, CTest e CI verdes.

## Registro do PR

Entregues `LocatedLine.hpp`, `PassOne.hpp` e `PassOne.cpp` no namespace
`assembler`, com testes em `PassOneTest.cpp` e documentação em
[pass-one.md](../pass-one.md). Nenhum contrato público existente dos pacotes
1, 2 e 3 ou da VM foi alterado.

Decisões implementadas:

- `PassOne::run` consome `SourceParseResult` ou `std::span<const SourceLine>`,
  recusando montagem parcial quando há falhas sintáticas ou linhas `Invalid`;
- consulta `findInstruction` e `parseInstructionOperand` do pacote 2 como fonte
  única dos 38 mnemônicos, formatos (`2`, `3` e `4`) e validação de operandos
  de instruções;
- utiliza `SymbolTable`, `parseValue`, `parseKnownInteger` e `referenceOperand`
  do pacote 3 para definir rótulos, registrar referências anteriores/futuras na
  ordem física e validar inteiros em `Address20BitRange`;
- avança `LOCCTR` com aritmética de 64 bits sem *wrap-around*, aceitando até o
  último byte válido (`0xFFFFF`) e reportando `AddressOutOfRange` a partir de
  `0x100000`;
- preserva `BASE`, `NOBASE` e comentários na ordem original com `sizeBytes = 0`;
- na entrega original do PR #27, rejeitava `BYTE`, `WORD`, `RESB` e `RESW` com
  `PassOneDiagnosticCode::DataDirectiveNotIntegrated`, sem definir o rótulo ou
  publicar uma linha de tamanho desconhecido. Esse comportamento foi substituído
  pela integração do pacote 5 no PR #28, documentada em [dados](../data.md);
- limites de endereço do layout e de `Address20BitRange` compartilham a definição
  em `AddressLimits.hpp`, sem alterar os valores ou as assinaturas do pacote 3.

Na revisão do PR #27, o teste de rejeição das quatro diretivas foi reforçado e
reproduziu a publicação indevida de símbolo/linha de tamanho zero antes da
correção. Após o ajuste, builds limpos Release passaram em 356 testes sem Qt e
359 com Qt, incluindo os 11 testes de `PassOneTest`. Formatação e
`git diff --check` passaram. A CI e o parecer externo são registrados no PR.
