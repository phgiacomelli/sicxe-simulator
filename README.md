# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido para a disciplina
de Programação de Sistemas.

O projeto está sendo construído em etapas. No estado atual, a memória, o
arquivo de registradores e o decoder de instruções já possuem implementação.
A resolução de endereços, a execução das instruções e o ciclo completo da
máquina ainda serão implementados.

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

- [Arquitetura](docs/architecture.md): módulos, responsabilidades e fluxo de
  uma instrução.
- [Decoder](docs/decoder.md): formatos reconhecidos, validações, resultados e
  falhas de decodificação.
- [Testes](docs/testing.md): como executar os testes.
- [Roadmap](docs/roadmap.md): estado atual e sequência sugerida para as próximas
  implementações

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
