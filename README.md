# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido para a disciplina
de Programação de Sistemas.

A primeira etapa, a VM, inclui memória, registradores, decoder, resolução de
operandos, as 38 instruções reconhecidas, ciclo atômico da máquina, entrada de
bytes codificados e interface Qt para execução e inspeção. Os programas de
aceitação e os testes da interface acompanham a implementação.

A segunda etapa entrega o [montador de duas passagens](docs/assembler/README.md),
com os sete pacotes implementados e o pipeline final validado no
[PR #30](https://github.com/phgiacomelli/sicxe-simulator/pull/30).
O parser de fonte, os contratos mínimos, os símbolos, o pacote de
[catálogo/operandos/formato 2](docs/assembler/isa.md) e a
[passagem 1 e contador de localização](docs/assembler/pass-one.md) têm implementação em C++20.
O catálogo também é a fonte única dos mnemônicos reservados na tabela de símbolos.
As [diretivas de dados e reservas](docs/assembler/data.md) também estão
implementadas e integradas ao contador. A [codificação das instruções](docs/assembler/encoding.md)
de formatos 2/3/4 está integrada à segunda passagem: `assemble()` gera a imagem
fixa e a listagem, e `SICXEAssembler` grava `.bin` e `.lst`. Veja o
[guia de uso](docs/assembler/usage.md). A VM continua recebendo apenas bytes.

A terceira etapa, **em desenvolvimento**, acrescentará o
[processador de macros](docs/macros/README.md) antes do montador, em uma
passagem, com sete pacotes de trabalho. O pacote 1 entrega scanner permissivo
e contratos compartilhados em `sicxe_macros`, sem Qt/VM/I/O. O pacote 2
acrescenta captura e tabela com versões imutáveis, com revisão técnica e CI
verde no [PR #32](https://github.com/phgiacomelli/sicxe-simulator/pull/32).
O pacote 3 acrescenta parsing de protótipo/chamada e vinculação posicional de
argumentos, com revisão técnica e CI verde no
[PR #33](https://github.com/phgiacomelli/sicxe-simulator/pull/33).
O pacote 4 entrega [substituição com escopo aninhado](docs/macros/contracts.md#interfaces-internas-entregues-no-pacote-4),
com templates imutáveis e definições internas preparadas para o futuro motor.
Os pacotes 5–7 continuam planejados:
`expand()` está somente declarada, sem motor ou CLI
de macros. Veja as [interfaces entregues](docs/macros/contracts.md#interfaces-c-entregues-no-pacote-1).

## Tecnologias

- C++20
- Qt6
- CMake

## 1ª Etapa - VM

- simulador da máquina SIC/XE;
- execução de instruções;
- memória e registradores;
- modos de endereçamento;
- interface gráfica para visualização da execução.

O recorte da VM reconhece somente as 38 instruções implementadas.
Instruções fora desse conjunto são tratadas como opcodes desconhecidos. A VM
recebe código de máquina já codificado; montador, linker e relocação não
fazem parte **da VM**. A referência a Java no enunciado é legada: o professor
deixou a escolha da linguagem ao grupo, que adotou C++20 e Qt 6. A decisão está
registrada na [arquitetura](docs/vm/architecture.md).

## Documentação

- [Índice da documentação](docs/README.md): VM, montador, macros, roadmap e
  testes.
- Enunciado original (`PRIMEIRO_TRABALHO_SIC_XE.pdf`, distribuído fora do Git):
  especificação fornecida para o trabalho; a indicação de Java foi substituída
  pela orientação do professor citada acima.
- [Arquitetura](docs/vm/architecture.md): módulos, responsabilidades e fluxo de
  uma instrução.
- [Decoder](docs/vm/decoder.md): formatos reconhecidos, validações, resultados e
  falhas de decodificação.
- [Endereçamento](docs/vm/addressing.md): operandos tipados, cálculos de endereço,
  Condition Code e falhas de resolução.
- [Contratos de execução](docs/vm/execution-contracts.md): efeitos atômicos,
  falhas estruturadas e regra do PC pós-fetch.
- [Transferência e lógica](docs/vm/execution-data.md): fontes, destinos, larguras,
  efeitos e falhas das operações do pacote 2.
- [Aritmética](docs/vm/execution-arithmetic.md): semântica signed de 24 bits,
  comparações, shifts e indexação do pacote 3.
- [Ciclo da máquina](docs/vm/machine-cycle.md): pipeline, validação e commit de
  efeitos, atomicidade, falhas, controle de fluxo, carga, reset e snapshot do
  pacote 4.
- [Entrada de programas](docs/vm/program-input.md): formatos, falhas, comandos e
  sinais do adaptador Qt do pacote 5.
- [Testes](docs/testing.md): como executar os testes.
- [Demonstração](docs/vm/demo.md): roteiro reproduzível da interface.
- [Roadmap](docs/roadmap.md): estado das etapas.
- [Pacotes de trabalho](docs/vm/work/README.md): divisão detalhada entre os seis
  integrantes da VM.
- [Segunda etapa — montador](docs/assembler/README.md): recorte, arquitetura de
  duas passagens e divisão entre sete integrantes.
- [Contratos do montador](docs/assembler/contracts.md): dialeto, interfaces,
  tipos mínimos entregues, codificação e saída implementadas.
- [Fonte do montador](docs/assembler/source.md): parser, campos, posições,
  diagnósticos e recuperação por linha implementados no pacote 1.
- [Catálogo da ISA, operandos e formato 2](docs/assembler/isa.md): tabela das 38
  instruções, validação de operandos e codificação de formato 2 do pacote 2.
- [Símbolos e valores de operandos](docs/assembler/symbols.md): tabela de
  rótulos locais, referências, resolução e leitura de inteiros/rótulos do
  pacote 3.
- [Passagem 1 e contador de localização](docs/assembler/pass-one.md): cálculo de
  endereços, tamanhos de instruções, `START`/`END`/`BASE`/`NOBASE` e
  intermediário `LocatedLine` do pacote 4.
- [Dados e reservas](docs/assembler/data.md): medição e codificação de
  `BYTE`/`WORD`/`RESB`/`RESW` do pacote 5 do montador.
- [Codificação das instruções](docs/assembler/encoding.md): formatos 2/3/4,
  prioridades de PC/base, limites e diagnósticos do pacote 6.
- [Uso do montador](docs/assembler/usage.md): CLI, segunda passagem, binário,
  listagem e programa de demonstração executável na VM, do pacote 7.
- [Terceira etapa — macros](docs/macros/README.md): objetivo, fluxo em uma
  passagem, fronteiras e estado dos sete pacotes.
- [Contratos de macros](docs/macros/contracts.md): sintaxe adotada,
  aninhamento, diagnósticos e integração com o montador existente.
- Slides do montador (`Montador.pdf`, distribuído fora do Git): referência do
  professor para a segunda etapa, com o recorte SIC/XE explicitado na
  documentação acima.
- Slides de macros (`ProcessadorDeMacros.pdf`, distribuído fora do Git):
  referência do professor para a terceira etapa; exemplos genéricos não
  ampliam a ISA SIC/XE do projeto.
- [Como contribuir](CONTRIBUTING.md): fluxo de branches, commits, revisão e
  critérios para integrar mudanças.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/SICXESimulator
```

É necessário ter CMake, compilador C++20 e Qt 6 (Core e Widgets). Para compilar
os testes, também são necessários Qt Test e GoogleTest; o CMake obtém
GoogleTest automaticamente quando não o encontra instalado.

Na janela, clique em **Carregar Programa**, escolha texto hexadecimal ou um
arquivo binário bruto, informe endereço de carga e PC inicial em hexadecimal e
confirme. Depois use **Executar Passo** ou **Executar**; **Pausar** interrompe a
execução contínua entre instruções. Registros, instrução no PC atual, memória e
diagnóstico são atualizados após cada passo. A memória pode ser consultada por
endereço inicial e tamanho da faixa. A VM não possui `HALT` nem parada automática
no fim dos bytes carregados: pause ou observe uma falha. Para resetar ou carregar
outro programa durante execução contínua, pause primeiro. Consulte o
[roteiro](docs/vm/demo.md) para um programa pronto para colar.

## Montar um programa

O build também cria `SICXEAssembler`, independente de Qt:

```bash
./build/SICXEAssembler tests/assembler/fixtures/sum-and-table.asm \
  -o build/sum-and-table.bin -l build/sum-and-table.lst
```

As saídas não podem existir previamente; os diretórios devem existir e
suportar hard links. A demonstração soma 1 a 5, dobra e adiciona 7: resultado
37. Carregue o `.bin` na interface com endereço de carga e PC inicial `100`
(hexadecimal); após 28 passos, A vale 37 e a palavra em `12C` contém 37.
Pause no laço final, pois não há `HALT`. Opções, limitações e APIs estão no
[guia de uso](docs/assembler/usage.md).

Para compilar montador e testes sem Qt:

```bash
cmake -S . -B build-core -DSICXE_BUILD_QT=OFF
cmake --build build-core --parallel 4
```

## Testes

```bash
ctest --test-dir build --output-on-failure
```

Os testes da interface rodam sem janela visível usando a plataforma Qt
`offscreen`, inclusive na CI. Para build e testes do núcleo sem Qt, veja
[entrada de programas](docs/vm/program-input.md#build-e-integração).

## Code Style

O projeto usa `clang-format` com as regras definidas em `.clang-format`.
Com `clang-format` instalado, o CMake cria o alvo `format`. Para formatar todos
os arquivos C++ manualmente:

```bash
cmake --build build --target format
```
