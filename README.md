# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido para a disciplina
de Programação de Sistemas.

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


## Build

```bash
cmake -S . -B build
cmake --build build
./build/SICXESimulator