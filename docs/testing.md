# Testes

## Ferramentas

Os testes usam GoogleTest e são registrados no CTest pelo CMake.

A suíte atual cobre `Memory`, `RegisterFile`, `Decoder` e `AddressResolver`. Os
testes do resolver incluem todos os modos aceitos pelo decoder, limites da
memória, indireção e garantia de ausência de mutação.

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

