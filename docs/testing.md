# Testes

## Ferramentas

Os testes usam GoogleTest e são registrados no CTest pelo CMake.

A suíte atual cobre `Memory`, `RegisterFile`, `Decoder`, `AddressResolver`, os
contratos de `ExecutionEffect` e as operações de transferência e lógica. Os
testes do resolver incluem todos os modos aceitos pelo decoder, limites da
memória, indireção e garantia de ausência de mutação. Os contratos de execução
possuem testes de construção, normalização e PC pós-fetch.

`DataOperationsTest.cpp` exercita individualmente as 18 instruções do pacote 2,
operandos imediatos e de memória, limites de byte e palavra, operandos
incompatíveis, preservação do estado antes do commit e ausência de alterações
no Condition Code.

Para configurar, compilar e executar:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Para listar os casos sem executá-los:

```bash
ctest --test-dir build -N
```
