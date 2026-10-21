# Pesquisa: convenções de macros e SIC/XE

## Conclusão

SIC/XE é uma **máquina didática hipotética**, não um computador comercial com
um único macro-montador oficial. Macros são uma transformação do **texto-fonte
antes da montagem**; seus delimitadores, parâmetros e rótulos locais não são
instruções da CPU nem formatos de bytes da arquitetura. A editora do livro de
referência trata *Assemblers* e *Macro Processors* em capítulos separados
([catálogo da Pearson, sumário do livro de Leland Beck][pearson]); a
[documentação do SicTools][sictools], uma implementação independente de
ferramentas SIC/XE, também lista suas próprias diretivas de montador, sem
estabelecer uma linguagem universal de macros.

**Não encontrei fonte primária que estabeleça `MCDEFN`/`MCEND`, a grafia dos
parâmetros ou `.SER` como sintaxe canônica do SIC/XE.** O PDF do professor é a
fonte primária **para esta disciplina**, e demonstra essas construções como
recursos de um processador de macros. Portanto, podem ser adotadas como o
**dialeto de macros deste projeto**, mas não devem ser descritas como comandos
obrigatórios da arquitetura SIC/XE. A ausência desses itens na lista de
diretivas do SicTools é apenas evidência de que outra ferramenta adota um
escopo próprio, não prova isolada de inexistência de outras convenções.

## Evidência por recurso

As referências de slide abaixo são os números impressos em
`ProcessadorDeMacros.pdf`, distribuído ao grupo fora do Git; o texto foi
conferido no próprio PDF. O enunciado complementar do professor, fornecido
ao grupo, exige processamento antes da montagem, **uma passagem**, definições
e chamadas aninhadas, ativação pelo integrador e saída `MASMAPRG.ASM`.

| Recurso | Evidência nos slides | Classificação para o projeto |
| --- | --- | --- |
| `MCDEFN`/`MCEND` | Slides 12–13: duas versões da definição usam esses delimitadores, mas posicionam o protótipo de maneiras diferentes. | Diretivas do dialeto de macros ensinado, não instruções SIC/XE. A versão do protótipo deve ser fixada no contrato. |
| Parâmetros posicionais | Slides 15–18: substituição por posição, inclusive omissão; o slide 18 cita o assembler IBM 360/370 como **alternativa**, não como regra SIC/XE. | Recurso de processador de macros exemplificado pelo professor. A sintaxe exata de omissão é escolha do contrato. |
| Parâmetros por palavra-chave | Slides 16, 19–20: correspondência `formal=actual`; o slide 20 apresenta mais de uma maneira de marcar o formal. | Outra abordagem de sintaxe demonstrada, não imposição da arquitetura. |
| Valores default | Slide 21: definição com valores por falta para palavras-chave e omissões. | Recurso demonstrado; a precedência e os erros de combinação precisam de contrato próprio. |
| Rótulo formal | Slides 12–14 e 19: um rótulo no protótipo pode receber o rótulo da chamada. | Parâmetro de macro que produz um rótulo assembly comum após expansão. |
| Serial `.SER` | Slides 22–23: uma **solução possível** para tornar rótulos distintos por expansão; o slide 23 chama `SER` de parâmetro formal definido pelo sistema. Slides 26 e 31 mostram uso/substituição. | Convenção de geração de rótulos do processador, não sintaxe que a VM ou o montador precisam reconhecer. O contrato deve dizer se é explícita e como é numerada. |
| Definições e chamadas aninhadas | Slides 32 e 35–45 explicam a implementação em uma passagem e os dois tipos de aninhamento. | Exigência explícita do professor no enunciado complementar. |

Os exemplos de código dos slides ilustram a **expansão textual**. Seus nomes de
operação não ampliam o conjunto de instruções autorizado pelo PDF da primeira
etapa; só a fonte expandida, sem construções de macro, segue para o montador
SIC/XE existente.

## Aplicação ao projeto

O [contrato](contracts.md) escolhe **uma** das alternativas exibidas:
`MCDEFN` em linha própria, protótipo na linha seguinte, parâmetros
**somente posicionais**, rótulo formal opcional e sufixo `.SER` explícito
para rótulos locais. Parâmetros por palavra-chave e defaults estão nos
slides, mas ficam fora desta entrega: não são exigidos pelo texto prioritário
do professor. O montador continua sem conhecer essas construções. A grafia
de identificadores, a vinculação de rótulos e a numeração do serial são
**convenções do projeto**, não requisitos da arquitetura SIC/XE.

[pearson]: https://in.pearson.com/content/dam/region-growth/india/pearson-india/Support/pdf/Computer%20Science%20Engineering.pdf
[sictools]: https://jurem.github.io/SicTools/documentation/directives.html
