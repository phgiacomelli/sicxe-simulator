# Pacote 7 — Passagem 2, saída e aceitação

Estado: planejado. Responsável: a definir.

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
`src/assembler/main.cpp`,
`tests/assembler/AssemblerAcceptanceTest.cpp`,
`tests/assembler/fixtures/`, `docs/assembler/usage.md` e CMake.
Atualizar `README.md` e `docs/testing.md` com comandos reais no PR.
Não alterar `MachineController`, widgets ou `Machine` para integrar.

## Interface a entregar

- `assemble(sourceText)` devolve `AssemblyResult` de
  [contratos](../contracts.md#interface-externa-e-intermediário), sem I/O
  e sem estado global.
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

- [ ] CLI reproduz montagem e saídas a partir de checkout limpo.
- [ ] Bytes/listagem/falhas batem com expectativas independentes.
- [ ] Aceitação demonstra compatibilidade com a VM sem mudar a VM.
- [ ] README, testes, uso, roadmap e índice atualizados.
- [ ] Build, formatação, CTest e CI verdes com e sem Qt.

## Registro do PR

Preencher ao implementar: decisões, divergências dos PDFs, interfaces
alteradas, testes e pendências.
