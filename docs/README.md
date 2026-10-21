# Documentação do projeto

Cada etapa tem seu próprio índice e seus pacotes de trabalho:

- [VM](vm/README.md): execução de bytes SIC/XE, aplicação Qt e interface.
- [Montador](assembler/README.md): montagem SIC/XE em duas passagens.
- [Processador de macros](macros/README.md): expansão em uma passagem antes
  da montagem; scanner/contratos do pacote 1 implementados e validados,
  expansão e integrador ainda planejados.

O [roadmap](roadmap.md) acompanha o estado das etapas. O [guia de
testes](testing.md) reúne os comandos e resultados de verificação do projeto.
As decisões, os contratos e as tarefas específicos de cada etapa ficam em
sua respectiva pasta, inclusive `work/`.
