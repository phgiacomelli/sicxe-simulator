# Testes

## Ferramentas

Os testes usam GoogleTest e QTest e são registrados no CTest pelo CMake.

A suíte atual cobre `Memory`, `RegisterFile`, `Decoder`, `AddressResolver`, os
contratos de `ExecutionEffect`, as três famílias de operações, o dispatcher, a
validação de efeitos e o ciclo completo da `Machine`. Os testes do resolver
incluem todos os modos aceitos pelo decoder, limites da memória, indireção e
garantia de ausência de mutação. Os contratos de execução possuem testes de
construção, normalização e PC pós-fetch.

`DataOperationsTest.cpp` exercita individualmente as 18 instruções do pacote 2,
operandos imediatos e de memória, limites de byte e palavra, operandos
incompatíveis, preservação do estado antes do commit e ausência de alterações
no Condition Code.

`ArithmeticOperationsTest.cpp` cobre os 14 opcodes do pacote 3, limites signed
de 24 bits, overflow com wrap, divisão e divisão por zero, os três resultados
de Condition Code, shifts de 1 e 16 bits e os efeitos duplos de `TIX` e `TIXR`.

`ControlOperationsTest.cpp` cobre os seis opcodes de controle, saltos tomados e
não tomados, retorno de sub-rotina, operandos incompatíveis e Condition Code
reservado. `ExecutorTest.cpp` verifica o despacho exato das 38 instruções e a
normalização de escritas em `PC`.

`EffectValidationTest.cpp` exercita limites e conflitos antes do commit.
`MachineTest.cpp` integra os módulos reais e cobre carga, reset, snapshots,
avanço de PC, as 38 instruções pelo ciclo completo, limites, falhas e ausência
de alterações parciais.

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

Os testes em `tests/application` cobrem os parsers hexadecimal e binário,
equivalência das entradas, erros de leitura, limites de carga, preservação da
sessão, comandos enfileirados, snapshots imutáveis, detalhes das falhas,
responsividade e pausa entre instruções. O teste Qt usa `QCoreApplication`,
sem necessidade de display. O build sem Qt é descrito em
[program-input.md](program-input.md#build-e-integração).

`tests/ui/InterfaceTest.cpp` usa QTest e a plataforma `offscreen` para verificar
os controles por estado, entrada hexadecimal e binária no diálogo, erros de
endereço de carga/PC e programa que ultrapassa a memória, instrução atual,
consulta de diferentes faixas e destaques. Os testes da janela exercitam carga,
passo, armazenamento visível na memória, falha persistente, reset e
pausa/retomada. Os testes de `MachineController` verificam a preservação da
sessão em cargas rejeitadas.

`tests/acceptance/ProgramAcceptanceTest.cpp` declara 18 programas com bytes
codificados, endereço de carga, PC inicial, limite de passos e estado final
esperado. Cada caso passa pelo parser, `MachineController` e `Machine`; a
comparação verifica todos os registradores, memória completa e código de falha.
O limite de passos é do teste, não da VM. Os cenários cobrem operações, modos de
endereçamento, desvios, sub-rotina, formato 4, shifts e falhas principais.
O [roteiro de demonstração](demo.md) reproduz o fluxo manual.

No workflow da CI, `InterfaceTests` recebe `QT_QPA_PLATFORM=offscreen` pelo
CTest, portanto não precisa de display. A CI remota do PR ainda precisa ser
confirmada. Para executar só os testes
novos:

```bash
ctest --test-dir build -R 'InterfaceTests|ProgramAcceptanceTests' --output-on-failure
```
