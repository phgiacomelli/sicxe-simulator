# Pacote 4 — Passagem 1 e contador de localização

Estado: implementado e validado localmente; revisão cruzada e CI remota
pendentes.

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
responsável por este pacote. Até essa integração, `BYTE/WORD/RESB/RESW`
devem receber erro explícito, nunca tamanho inventado.

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
- rejeição explícita de diretiva de dados antes do pacote 5.

## Fora do escopo

Não calcular PC/base, codificar opcodes, emitir binário ou implementar
`EQU`, `ORG`, `LTORG`, pools, múltiplas seções ou várias localizações.
Não carregar bytes na `Machine`.

## Critério de conclusão

- [x] Cada instrução tem endereço/tamanho estáveis para a segunda passagem.
- [x] `START`, `END`, labels, limites e erros possuem testes.
- [x] `docs/assembler/pass-one.md` mostra um intermediário SIC/XE real.
- [x] Build, formatação e CTest locais verdes com e sem Qt.
- [ ] CI remota verde e revisão cruzada.

## Registro do PR

Entregues `LocatedLine.hpp`, `PassOne.hpp` e `PassOne.cpp` no namespace
`assembler`, com testes em `tests/assembler/PassOneTest.cpp` e documentação em
[pass-one.md](../pass-one.md). Nenhum contrato público existente do pacote 1
ou da VM foi alterado.

Decisões implementadas:

- `PassOne::run` aceita `SourceParseResult` ou `std::span<const SourceLine>` e
  interrompe imediatamente com `InvalidSourceLine` quando o parser reportou
  falha sintática ou linha `Invalid`, sem tentar montagem parcial;
- `LocatedLine` preserva a `SourceLine` original e fixa `category`
  (`Instruction`, `Data`, `Reservation`, `Directive` e `Comment`), `address`
  em bytes, `size`, mnemônico normalizado, flag `extended`, `opcode`,
  `instructionFormat` (`Format2`, `Format3`, `Format4`) e `directive`;
- o contador de localização usa aritmética larga de 64 bits (`std::uint64_t`)
  e valida `0x00000..0xFFFFF`, inclusive o último byte (`locationCounter + size <= 0x100000`),
  sem wrap e interrompendo o avanço após tamanho inválido ou overflow;
- rótulos recebem o contador antes do avanço e são exportados em
  `PassOneResult::symbols` (`std::vector<AssemblySymbol>`) na ordem
  determinística de definição, com integração automática a `SymbolTable` e
  `OperandValue` do pacote 3 quando presentes;
- referências anteriores e futuras em operandos de instruções de memória,
  `BASE` e `END` são registradas sem alterar o tamanho fixado na passagem 1, e
  símbolos não definidos geram `UndefinedSymbol` ao final da passagem;
- `BYTE`, `WORD`, `RESB` e `RESW` são classificados em `Data`/`Reservation` e
  rejeitados explicitamente com `DataDirectiveNotIntegrated` sem inventar
  tamanho até a integração do pacote 5.

