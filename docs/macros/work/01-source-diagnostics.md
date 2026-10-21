# Pacote 1 — fonte de macros, diagnósticos e seam público

Responsável: um integrante. Estado: **planejado**. Merge primeiro.
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
sucesso ou falha, sem I/O e sem estado entre chamadas. A implementação pode
começar como cópia de linhas comuns; outros pacotes acrescentam o motor.
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
  somente os finais de linha do resultado para LF. Em chamada de macro,
  separar o comentário inline para emissão como linha de comentário antes
  da expansão; comentários em linhas estruturais não são emitidos.
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
