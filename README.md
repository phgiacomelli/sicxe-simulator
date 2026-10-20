# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido para a disciplina
de Programação de Sistemas.

A primeira etapa, a VM, inclui memória, registradores, decoder, resolução de
operandos, as 38 instruções reconhecidas, ciclo atômico da máquina, entrada de
bytes codificados e interface Qt para execução e inspeção. Os programas de
aceitação e os testes da interface acompanham a implementação.

A segunda etapa está em desenvolvimento em [montador de duas passagens](docs/assembler/README.md).
O parser de fonte, os contratos mínimos, os símbolos, o pacote de
[catálogo/operandos/formato 2](docs/assembler/isa.md) e a
[passagem 1 e contador de localização](docs/assembler/pass-one.md) têm implementação em C++20.
O catálogo também é a fonte única dos mnemônicos reservados na tabela de símbolos.
As [diretivas de dados e reservas](docs/assembler/data.md) também estão
implementadas e integradas ao contador. A [codificação das instruções](docs/assembler/encoding.md)
de formatos 2/3/4 está implementada; segunda passagem e geração de programas
ainda dependem do pacote 7.

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
registrada na [arquitetura](docs/architecture.md).

## Documentação

- Enunciado original (`PRIMEIRO_TRABALHO_SIC_XE.pdf`, distribuído fora do Git):
  especificação fornecida para o trabalho; a indicação de Java foi substituída
  pela orientação do professor citada acima.
- [Arquitetura](docs/architecture.md): módulos, responsabilidades e fluxo de
  uma instrução.
- [Decoder](docs/decoder.md): formatos reconhecidos, validações, resultados e
  falhas de decodificação.
- [Endereçamento](docs/addressing.md): operandos tipados, cálculos de endereço,
  Condition Code e falhas de resolução.
- [Contratos de execução](docs/execution-contracts.md): efeitos atômicos,
  falhas estruturadas e regra do PC pós-fetch.
- [Transferência e lógica](docs/execution-data.md): fontes, destinos, larguras,
  efeitos e falhas das operações do pacote 2.
- [Aritmética](docs/execution-arithmetic.md): semântica signed de 24 bits,
  comparações, shifts e indexação do pacote 3.
- [Ciclo da máquina](docs/machine-cycle.md): pipeline, validação e commit de
  efeitos, atomicidade, falhas, controle de fluxo, carga, reset e snapshot do
  pacote 4.
- [Entrada de programas](docs/program-input.md): formatos, falhas, comandos e
  sinais do adaptador Qt do pacote 5.
- [Testes](docs/testing.md): como executar os testes.
- [Demonstração](docs/demo.md): roteiro reproduzível da interface.
- [Roadmap](docs/roadmap.md): estado das etapas.
- [Pacotes de trabalho](docs/work/README.md): divisão detalhada entre os seis
  integrantes da VM.
- [Segunda etapa — montador](docs/assembler/README.md): recorte, arquitetura de
  duas passagens e divisão entre sete integrantes.
- [Contratos do montador](docs/assembler/contracts.md): dialeto, interfaces,
  tipos mínimos entregues, codificação e saída planejadas.
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
- Slides do montador (`Montador.pdf`, distribuído fora do Git): referência do
  professor para a segunda etapa, com o recorte SIC/XE explicitado na
  documentação acima.
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
[roteiro](docs/demo.md) para um programa pronto para colar.

## Testes

```bash
ctest --test-dir build --output-on-failure
```

Os testes da interface rodam sem janela visível usando a plataforma Qt
`offscreen`, inclusive na CI. Para build e testes do núcleo sem Qt, veja
[entrada de programas](docs/program-input.md#build-e-integração).

## Code Style

O projeto usa `clang-format` com as regras definidas em `.clang-format`.
Com `clang-format` instalado, o CMake cria o alvo `format`. Para formatar todos
os arquivos C++ manualmente:

```bash
cmake --build build --target format
```
