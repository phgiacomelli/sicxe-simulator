# Pacote 7 — Passagem 2, saída e aceitação

Estado: implementado e validado localmente; revisão cruzada e CI remota
pendentes antes do merge. Branch: `feat/assembler-pass-two`.

## Resultado esperado

Integrar os módulos anteriores **sem reimplementar suas regras**:
percorrer o intermediário da passagem 1, aplicar `BASE`/`NOBASE`,
codificar instruções/dados e gerar `.bin` bruto e `.lst`. Expor um CLI
simples e verificar ponta a ponta que os bytes montados executam na VM
existente no endereço informado.

## Dependências e ownership

Depende dos pacotes 1–6. Arquivos sugeridos:
`src/assembler/Assembler.*`, `src/assembler/PassTwo.*`,
`src/assembler/RawImageWriter.*`, `src/assembler/ListingWriter.*`,
`src/assembler/AssemblerCli.*`, `src/assembler/OutputFiles.*`,
`src/assembler/main.cpp`,
`tests/assembler/AssemblerTest.cpp`, `tests/assembler/AssemblerCliTest.cpp`,
`tests/assembler/AssemblerAcceptanceTest.cpp`,
`tests/assembler/CliSmoke.cmake`, `tests/assembler/fixtures/`,
`docs/assembler/usage.md` e CMake.
Atualizar `README.md` e `docs/testing.md` com comandos reais no PR.
Não alterar `MachineController`, widgets ou `Machine` para integrar.

## Interface a entregar

- `assemble(sourceText)` devolve `AssemblyResult` de
  [contratos](../contracts.md#interface-externa-e-intermediário), sem I/O
  e sem estado global.
  Os tipos `assembler::AssemblySuccess`, `AssemblyFailure`,
  `AssemblyListingLine` e `AssemblySymbol` já existem desde o pacote 1;
  consultar [source.md](../source.md) e validar as invariantes no produtor.
- Writer `.bin`: imagem fixa de `START` até o último byte usado,
  incluindo zero em reservas. Writer `.lst`: fonte, número de linha,
  endereço, bytes ou reserva e diagnósticos. Não há `.obj` nem registros
  para loader.
- CLI recebe caminho da fonte e caminhos de saída, reporta
  arquivo/linha/coluna, retorna erro não zero e não deixa arquivos de
  sucesso parciais. Exibe `START` e PC inicial para carga na VM.

## Comportamento obrigatório

Consumir `LocatedLine` em ordem, manter estado local de `BASE`/`NOBASE`
e comparar tamanho codificado ao tamanho fixado na passagem 1. Dados
e instruções ocupam seus endereços; reservas são preenchidas apenas na
imagem final, sem alterar a listagem. Diagnóstico de qualquer pacote
impede `.bin`/`.lst` de sucesso. A VM carrega `.bin` no mesmo `START`;
nenhum registro ou endereço é interpretado por um novo loader. CLI
e biblioteca do montador são C++20 sem Qt.

## Testes obrigatórios

- programa válido com referência futura, `START`/`END`, dados,
  reserva, PC/base e formato 4; bytes e endereços esperados à mão;
- casos separados de rótulo duplicado/indefinido, opcode vermelho,
  operando inválido e deslocamento fora da faixa;
- `.bin` com zeros nas reservas e PC/endereço de carga corretos;
- `.lst` com linha, comentário, endereço e bytes/reserva;
- erro sintático, semântico ou de I/O sem arquivos de sucesso parciais;
- programa montado carregado via **bytes** em `Machine` e estado final
  esperado calculado manualmente;
- build/testes com Qt e com `SICXE_BUILD_QT=OFF`.

## Fora do escopo

Não implementar loader `.obj`, linker, relocação, editor assembly,
montador de um passo, *load-and-go*, recursos genéricos excluídos no
índice ou novas instruções da VM. Não duplicar parser, símbolos ou
encoder para resolver falha de integração: corrigir no pacote dono.

## Critério de conclusão

- [x] CLI reproduz montagem e saídas em builds locais novos, com fixture versionada.
- [x] Bytes/listagem/falhas batem com expectativas independentes.
- [x] Aceitação demonstra compatibilidade com a VM sem mudar a VM.
- [x] README, testes, uso, roadmap e índice atualizados.
- [x] Build, formatação e CTest locais verdes com e sem Qt.
- [ ] CI remota verde e revisão cruzada no PR.

## Registro do PR

### Implementação e decisões

- `assemble()` compõe parser, passagem 1 e `PassTwo`, sem I/O ou estado global.
  Uma falha expõe somente `AssemblyFailure`, nunca bytes/listagem de sucesso.
- `PassTwo` usa `BASE`/`NOBASE` em ordem, aceita base zero, resolve referências
  pela tabela final, delega aos encoders e confere tamanhos sem modificar o
  intermediário. Não reimplementa parser, símbolos, dados ou endereçamento.
- `RawImageWriter` valida extensão/layout/faixa e preenche reservas com zero
  somente na imagem final; `ListingWriter` preserva fonte/comentários e mostra
  endereço, bytes/reserva e símbolos. Diagnósticos são formatados separadamente
  para stderr; não se grava `.lst` de sucesso quando a montagem falha.
- A CLI C++20 `SICXEAssembler` lê a fonte, recebe destinos opcionais, informa
  carga/PC e retorna `0`/`1`/`2`. `sicxe_assembler_cli` separa I/O do pipeline
  puro `sicxe_assembler`; nenhum deles depende de Qt ou da VM.
- Saídas existentes não são sobrescritas. Os dois conteúdos são preparados
  antes da publicação por hard links, que exige suporte no sistema de arquivos
  dos destinos. Falha na segunda publicação desfaz a primeira; falha adicional
  no desfazimento informa recuperação manual. Não há garantia transacional
  contra desligamento abrupto. Motivo: preservar arquivos do usuário e evitar
  publicar conteúdo truncado; não usar sobrescrita/cópia destrutiva como fallback.
- A demonstração versiona apenas a fonte `.asm`; binários/listagens gerados
  ficam em `build/`. Não foram alterados `core`, `application` ou `ui`.

### Interfaces e escopo

`AssemblyResult` e os contratos dos pacotes 1–6 foram preservados. Os códigos
`EmptyProgram` e `LayoutMismatch` foram acrescentados a `AssemblyDiagnosticCode`.
As novas APIs de pipeline, writers e CLI estão detalhadas em
[uso](../usage.md#apis-e-responsabilidade-de-cada-módulo). Erros de arquivo têm
`OutputDiagnosticCode` separado dos diagnósticos da fonte.

Nenhuma nova interpretação dos PDFs: foram seguidos o recorte SIC/XE e as
convenções mínimas já acordadas em [contratos](../contracts.md). Nada de
instruções vermelhas, loader/linker, relocação, expressões ou editor assembly.

### Testes e pendências

26 testes novos: 11 de pipeline/writers, 11 de CLI com arquivos reais, dois
de aceitação e dois do executável pelo CTest. Cobrem falhas sintáticas,
semânticas, tardias e de arquivo, limites de memória, estado de base,
listagem exata e proteção dos destinos existentes/inválidos.

`sum-and-table.asm` tem bytes calculados manualmente, inclui referência futura,
subrotina, PC/base/formato 4 e reserva de 4096 bytes. O teste compara a imagem
de 4160 bytes e toda a memória/registradores relevantes após 28 passos reais
na VM. Outro caso monta as 38 instruções pela API final e confere o decoder.

Builds locais novos passaram em **411 testes sem Qt** (Debug) e **414 com Qt**
(Release), nenhum ignorado. Formatação de todos os C++ e compilação dos novos
fontes com `-Wall -Wextra -Wpedantic -Werror` também passaram. Comandos e detalhes
em [testes](../../testing.md#montador-pacote-7).

Pendências de fluxo: revisão por outro integrante e CI remota no futuro PR.
Registrar o link do PR e o resultado da CI quando estiverem disponíveis.
