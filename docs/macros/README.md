# Terceira etapa — processador de macros SIC/XE

Estado: **pacote 1 implementado e validado; pacotes 2–7 planejados**.
Scanner e contratos disponíveis em `sicxe_macros`; motor e CLI ainda não
implementados. Revisão técnica e CI verde do pacote 1 registradas no
[PR #31](https://github.com/phgiacomelli/sicxe-simulator/pull/31). Este plano
usa `ProcessadorDeMacros.pdf` (distribuído à equipe fora do Git) e a orientação
complementar do professor. O [contrato comum](contracts.md) distingue
exigências do professor de convenções adotadas para esta entrega. A separação
em novo executável, a política de `MASMAPRG.ASM` e a sintaxe mínima estão
fixadas no contrato para orientar os sete pacotes. Ver a
[pesquisa sobre convenções SIC/XE](research-sicxe-conventions.md).

## Objetivo e fronteira

O módulo principal do **macro-montador** recebe um arquivo-fonte SIC/XE,
processa macros **em uma passagem e antes das duas passagens do montador**,
publica a fonte expandida com o nome fixo `MASMAPRG.ASM` e então a entrega ao
montador existente. O processador permite **definições dentro de definições**
e **chamadas dentro de expansões**. A VM permanece inalterada: só recebe bytes
do `.bin` gerado pelo montador.
O grupo aprovou criar `SICXEMacroAssembler` para esse fluxo, preservando
`SICXEAssembler` para montagem direta.

```text
arquivo-fonte → processador de macros (1 passagem) → MASMAPRG.ASM
                                                    ↓
                                         montador SIC/XE (2 passagens)
                                                    ↓
                                             .bin + .lst → VM
```

O processador conhece a **sintaxe de macros** e preserva o assembly comum;
não calcula endereços, não resolve símbolos SIC/XE e não codifica opcodes.
O montador mantém sua interface `assemble(std::string_view)` e não passa a
entender `MCDEFN`, `MCEND` ou chamadas de macro. Os exemplos `LOAD`, `STORE`,
`BRPOS` e outros comandos didáticos dos slides não ampliam as 38 instruções
da nossa VM. Depois da expansão, a fonte deve obedecer exatamente ao
[dialeto do montador](../assembler/contracts.md).

## Método de uma passagem

O fluxo lê a fonte em ordem. Fora de definições, copia linhas comuns,
registra `MCDEFN` e expande uma chamada quando seu nome já está definido.
Durante uma expansão, cada linha gerada retorna ao mesmo fluxo de leitura:
ela pode definir outra macro, chamar outra macro ou ser copiada para a saída.
Uma definição interna **não entra na tabela** quando a externa é lida; ela
entra quando a expansão da externa a produz. A contagem de `MCDEFN`/`MCEND`
impede que o primeiro `MCEND` interno feche a definição externa.

Por ser uma passagem, não há busca posterior por definições: uma chamada
antes da definição visível fica sem expansão e chega ao montador, que rejeita
a operação desconhecida. Uma redefinição válida substitui a versão anterior
somente para chamadas posteriores, sem alterar texto já emitido.
Recursão direta/indireta de chamadas é diagnosticada, em vez de travar o
processamento.

## Sete pacotes, sete responsáveis

Cada pacote inclui implementação, testes, documentação afetada e um PR próprio.
Dependências abaixo são **ordem de integração**; os integrantes podem preparar
testes e implementação em paralelo nas suas branches.

| Pacote | Responsabilidade principal | Depende de | Estado |
| --- | --- | --- | --- |
| [1 — Fonte e diagnósticos](work/01-source-diagnostics.md) | Leitura lexical das linhas de macro, posições, tipos e contrato público mínimo | Nenhum; primeiro merge | Implementado; revisado e validado |
| [2 — Definições](work/02-definitions.md) | Captura por nível, tabela, definições internas e redefinição | 1 | Planejado |
| [3 — Argumentos](work/03-arguments.md) | Parâmetros formais e reais posicionais, omissão e diagnósticos | 1 | Planejado |
| [4 — Substituição](work/04-substitution.md) | Referências formais com escopo aninhado e renderização do esqueleto | 1, 2, 3 | Planejado |
| [5 — Contexto de expansão](work/05-local-labels.md) | `.SER`, serial determinístico, colisões e rastreabilidade das linhas geradas | 1, 4 | Planejado |
| [6 — Motor de uma passagem](work/06-expansion-engine.md) | Modos de cópia/definição/expansão, chamadas aninhadas e falhas de ciclo | 1–5 | Planejado |
| [7 — Integrador e aceitação](work/07-integration.md) | CLI do macro-montador, `MASMAPRG.ASM`, ligação com montador e testes ponta a ponta | 1–6 | Planejado |

Após 1, os pacotes 2 e 3 podem ser integrados independentemente. Depois:
4 → 5 → 6 → 7. O pacote 7 pode preparar a CLI e os cenários antes, mas só
integra o fluxo final com os módulos anteriores estáveis. O pacote 6 não deve
absorver I/O, e o 7 não deve reimplementar expansão ou montagem.

As [interfaces C++ do pacote 1](contracts.md#interfaces-c-entregues-no-pacote-1)
descrevem scanner, posições, diagnósticos e origem. `expand()` está apenas
declarada para o pacote 6 implementar; `normalizedText()` é cópia lexical,
não expansão. Ainda não é possível gerar `MASMAPRG.ASM` pelo projeto.

## Critério de conclusão

Todos os PRs preservam `main` compilável, usam testes pelas interfaces do
pacote, passam por CMake/CTest e `clang-format`, atualizam a documentação
afetada e recebem revisão de outro integrante. O PR final demonstra:

- definição e chamada comuns, parâmetros, redefinição e omissão;
- definição aninhada que só se torna visível após expandir a externa;
- chamada aninhada com substituições e serial de rótulos corretos;
- chamada anterior à definição sem expansão, rejeitada pelo montador;
  diagnósticos de macro para delimitadores inválidos e ciclos;
- `MASMAPRG.ASM` sem construções de macro, montável pelo montador existente;
- bytes equivalentes aos de uma fonte expandida manualmente e execução na VM
  **somente pelos bytes**, sem modificar `src/core`, `src/application` ou `src/ui`.

O roteiro de cada PR é o mesmo de [CONTRIBUTING](../../CONTRIBUTING.md): branch
curta desde `main`, ownership claro, testes e docs no próprio PR, revisão
cruzada e CI verde antes do merge.
