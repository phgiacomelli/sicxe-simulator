# Pacote 1 — fonte de macros, diagnósticos e seam público

Responsável: um integrante. Estado: **implementado localmente**, pendente de
revisão cruzada e CI remota. Merge primeiro.
Ler [índice](../README.md), [contratos](../contracts.md),
[gramática do montador](../../assembler/contracts.md) e `CONTRIBUTING.md`.

## Entrega e ownership

Criar os tipos mínimos de `src/macros/`: linha física, campos reconhecidos,
posição, origem, diagnóstico e resultado de expansão. Criar um scanner de
linhas de macro que identifica rótulo/operação/operando/comentário sem
interpretar opcodes SIC/XE. Ele deve aceitar as mesmas convenções de espaços,
comentários e finais de linha do montador, mas **não pode usar o parser de
assembly para rejeitar corpos de macro ainda não expandidos** (`.SER` e
parâmetros podem ser texto temporariamente inválido para o montador).
Linhas comuns também não são rejeitadas por sintaxe de assembly: essa
validação pertence ao montador após a expansão.

Fixar uma interface pequena: `macro::expand(sourceText)` terá resultado de
sucesso ou falha, sem I/O e sem estado entre chamadas. Neste pacote ela está
**somente declarada**: a implementação pertence ao motor do pacote 6.
`SourceScanResult::normalizedText()` oferece cópia lexical com LF, mas não
declara sucesso de expansão nem remove diretivas/comentários estruturais.
Não expor tabela de definições/pilha de expansão ao integrador. Criar
`tests/macros/SourceScannerTest.cpp` e registrar biblioteca/testes em CMake.

## Regras verificáveis

- LF, CRLF e CR; linha/coluna física de byte iniciada em 1; nenhuma linha
  fictícia ao fim da fonte.
- `.` na primeira posição não branca inicia comentário inteiro; `;` fora de
  aspas simples inicia comentário inline; vírgulas e `;` dentro de aspas não
  encerram campos.
- Reconhecer `MCDEFN`/`MCEND` só no campo de operação, independentemente de
  caixa; não confundir rótulo ou operando com diretiva.
- Preservar texto de linhas comuns e comentários na ordem original; normalizar
  somente os finais de linha da cópia lexical para LF. Separar o comentário
  inline, deixando sua emissão como linha anterior a uma chamada para o motor.
  A omissão de comentários estruturais também ocorre nos pacotes 2/6, não no
  scanner; ele precisa preservá-los para os consumidores decidirem.
- Resultado e diagnósticos possuem as strings; testes podem destruir o texto
  de entrada antes de consultar o resultado.

## Não fazer

Não capturar definições, vincular argumentos, expandir chamadas, abrir arquivos,
alterar `SourceParser`, `assembler::assemble` ou qualquer módulo da VM.

## Aceite do PR

Testes de scanner cobrem os casos acima, campos malformados relevantes e
comentários em protótipos. CMake e CTest com/sem Qt passam; formatação passa.
Documentar os tipos e posições finais em [contratos](../contracts.md). No PR,
avisar os pacotes 2, 3 e 6 sobre qualquer alteração da interface compartilhada.

## Registro da implementação

### Arquivos e decisões

- Biblioteca `sicxe_macros` com `SourcePosition.hpp`, `SourceLine.hpp/.cpp`,
  `SourceScanner.hpp/.cpp`, `SourceOrigin.hpp`, `MacroDiagnostic.hpp`,
  `MacroResult.hpp` e `MacroProcessor.hpp`. Não depende de Qt, VM ou montador.
- `SourceScanner::scan` lê linhas físicas, campos e comentários; retorna
  `SourceScanResult`. `SourceLine::directive()` reconhece os dois delimitadores
  pelo token exato de operação, sem duplicar uma classificação armazenada.
- `Code` significa linha com texto de código, não instrução válida. Os campos
  são opcionais e textuais; `.SER`, parâmetros, operações desconhecidas e
  assembly malformado não são rejeitados pelo scanner. `openQuote` informa
  aspa aberta para validação contextual; não carrega estado entre linhas.
- `terminated` permite normalizar apenas os terminadores existentes, sem
  inventar quebra final, eliminar linhas vazias ou alterar espaços/grafia.
- Resultado/diagnóstico/origem possuem strings e vetores. Cadeia de chamadas
  representa a ordem externa → interna; os produtores ainda serão implementados.
- `MacroProcessor.hpp` declara o seam final, sem stub linkável de `expand()`.
  Evita apresentar uma cópia com macros ainda presentes como expansão válida.
- Nenhum arquivo de `src/assembler`, `src/core`, `src/application` ou `src/ui`
  foi alterado; nenhum arquivo de saída ou executável de macros foi criado.

As formas C++ e suas invariantes ficam somente em
[contratos](../contracts.md#interfaces-c-entregues-no-pacote-1). Não há nova
interpretação dos PDFs nem extensão do dialeto acordado.

### Testes

`SourceScannerTest.cpp` tem 12 testes pela interface do scanner: campos opacos,
finais de linha, posições, comentários, reconhecimento dos delimitadores,
normalização sem expansão, campos inválidos/incompletos, aspas abertas,
marcadores SIC/XE, UTF-8/tabs como bytes, duração dos dados e NUL embutido.

`MacroContractTest.cpp` tem dois testes dos valores públicos: sucesso com
origens de linhas comuns/aninhadas e falha sem payload de sucesso; ambos
verificam propriedade dos dados. Uma expressão não avaliada confere o tipo de
retorno declarado de `expand()`, sem chamar ou simular o motor do pacote 6.

O alvo `MacroTests` usa apenas `sicxe_macros` e GoogleTest. A validação completa
e os comandos ficam em [testes](../../testing.md#macros-pacote-1).
Builds novos passaram em **425 testes sem Qt** (Debug) e **428 com Qt**
(Release), nenhum ignorado. Formatação de todos os C++, headers isolados e
novos fontes com avisos tratados como erro também passaram. A biblioteca
compilou separadamente com Qt e testes desligados.

### Handoff e conclusão

- [x] Scanner, posições e tipos mínimos implementados e testados.
- [x] Declaração de `expand()` disponível, sem implementação antecipada.
- [x] Interfaces e estado dos documentos atualizados.
- [x] Build, CTest e formatação locais aprovados com e sem Qt.
- [ ] Revisão cruzada e CI remota no PR.

No PR, pedir conferência dos campos ao pacote 2 (captura) e ao 3 (argumentos),
da origem ao pacote 5 e do resultado ao pacote 6. Diagnósticos semânticos,
pareamento de níveis, vínculo, serial e pilha continuam com seus responsáveis.
Os demais pacotes não estão implementados por esta entrega.
