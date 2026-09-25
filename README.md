# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido para a disciplina
de Programação de Sistemas.

O projeto está sendo construído em etapas. No estado atual, a memória, o
arquivo de registradores, o decoder de instruções, a resolução de operandos e
as 18 operações de transferência e lógica e as 14 operações aritméticas já
possuem implementação. As seis operações de controle de fluxo, o dispatcher e o
ciclo completo e atômico da máquina (carga, reset, snapshot e passo a passo)
também estão implementados. A entrada de programas, o adaptador Qt e a
interface gráfica ainda serão implementados.

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

O recorte da VM reconhece somente as 38 instruções que serão implementadas.
Instruções fora desse conjunto são tratadas como opcodes desconhecidos. A VM
receberá código de máquina já codificado; montador, linker e relocação não
fazem parte desta etapa.

## Documentação

- [Enunciado original](PRIMEIRO_TRABALHO_SIC_XE.pdf): especificação fornecida
  para o trabalho.
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
- [Testes](docs/testing.md): como executar os testes.
- [Roadmap](docs/roadmap.md): estado atual e sequência sugerida para as próximas
  implementações.
- [Pacotes de trabalho](docs/work/README.md): divisão detalhada entre os seis
  integrantes.
- [Como contribuir](CONTRIBUTING.md): fluxo de branches, commits, revisão e
  critérios para integrar mudanças.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/SICXESimulator
```

## Testes

```bash
ctest --test-dir build --output-on-failure
```

## Code Style

O projeto usa `clang-format` com as regras definidas em `.clang-format`.
Para formatar todos os arquivos C++ manualmente:

```bash
cmake --build build --target format
```
