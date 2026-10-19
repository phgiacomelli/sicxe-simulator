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
CTest, portanto não precisa de display. A CI executa esses testes em modo
offscreen. Para executar só os testes
novos:

```bash
ctest --test-dir build -R 'InterfaceTests|ProgramAcceptanceTests' --output-on-failure
```

## Montador: pacote 1

O alvo `AssemblerTests` liga somente `sicxe_assembler` e GoogleTest.
`tests/assembler/SourceParserTest.cpp` cobre 22 casos de linhas físicas,
campos, colunas, comentários, aspas, prefixos, vírgulas, recuperação após
erro e independência/vida útil dos resultados. O parser é somente lexical;
os testes também confirmam que ele deixa validações semânticas aos futuros
catálogo, tabela de símbolos e codificadores.

`tests/assembler/AssemblyContractTest.cpp` tem dois casos de consumo dos
tipos de sucesso (imagem, listagem, reserva e símbolos) e falha (somente
diagnósticos). Esses casos não afirmam que a montagem completa exista.

Os 24 testes novos são registrados no CTest nas configurações com e sem
Qt. Para executá-los após o build:

```bash
ctest --test-dir build -R 'SourceParserTest|AssemblyContractTest' --output-on-failure
```

A validação local do pacote 1 passou nos **247 testes com Qt**
(`build/assembler-task1`, Debug) e nos **244 sem Qt**
(`build/assembler-task1-core`, Release), em diretórios de build novos.
As configurações reutilizaram a fonte GoogleTest existente em `build/_deps`
por `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, sem instalação de dependências.
O `clang-format --dry-run --Werror` passou em todo `src/` e `tests/`,
e o parser compilou com `-Wall -Wextra -Wpedantic -Werror`.
A CI do PR do pacote 1 passou. A revisão cruzada dos contratos foi
dispensada na integração, mas permanece recomendada aos consumidores.

Para build sem Qt, use a configuração de
[program-input.md](program-input.md#build-e-integração); ela também compila
o montador. O comportamento do parser está em
[fonte do montador](assembler/source.md).

## Montador: pacote 3

`tests/assembler/SymbolTableTest.cpp` e `tests/assembler/OperandValueTest.cpp`
exercitam a tabela de rótulos locais e a leitura/validação de operandos
numéricos e simbólicos no alvo `AssemblerTests`:

- definição, uso anterior e uso futuro com múltiplas referências ao mesmo rótulo;
- normalização de caixa, rejeição de nomes inválidos e colisão com os 38
  mnemônicos e as 8 diretivas;
- redefinição com as duas posições e símbolo indefinido com posição do uso;
- conversão dos erros para `AssemblyDiagnostic` e preservação da grafia da
  primeira definição na mensagem de duplicidade;
- inteiros decimais e hexadecimais (`0x`/`0X`), zero, limites, overflow e
  faixas por diretiva (`Address12BitRange`, `Address20BitRange`, `Word24BitRange`);
- rejeição de expressões (`A+1`, `A-B`, `2*B`), sinais em símbolos e parênteses;
- isolamento entre montagens sucessivas.

Para executar os testes do pacote 3 após o build:

```bash
ctest --test-dir build -R 'SymbolTableTest|OperandValueTest' --output-on-failure
```

Após a revisão do PR #24, o build limpo e a suíte completa passaram em
**261 testes sem Qt** e **264 com Qt**; `clang-format --dry-run --Werror`
e `git diff --check` também passaram.

As regras e os códigos de diagnóstico estão em
[símbolos do montador](assembler/symbols.md).


## Montador: pacote 2

`InstructionCatalogTest`, `InstructionOperandTest` e `Format2EncoderTest`
rodam em `AssemblerTests`, que continua ligando somente `sicxe_assembler` e
GoogleTest. `AssemblerDecoderTests` contém os testes cruzados de
`Format2DecoderTest` e `InstructionCatalogDecoderTest`; somente esse alvo
liga também `SICXECore`. Ambos funcionam sem Qt.

Os 81 testes novos cobrem a tabela independente exata das 38 instruções,
formatos/tamanhos, consulta imutável, operandos de cada família, tokens e
posições próprias, registradores, contagens 1–16, restrições de indexação,
`+`, `RSUB`, literais/expressões recusados e falhas com código/posição.
Os testes cruzados fazem 656 round-trips dos dez opcodes de formato 2,
incluindo todos os oito registradores válidos nas posições aplicáveis e
os 16 nibbles de shift. Os bytes dos testes de memória são probes manuais
para verificar o catálogo; não há encoder de formatos 3/4 neste pacote.

Validação local em diretórios novos, com GCC 14.2, CMake 3.31.6, Qt 6.8.2,
GoogleTest 1.16 e clang-format 19 (ferramentas do ambiente Debian 13):

```bash
# A partir da raiz do repositório, com as ferramentas no PATH:

cmake -S . -B build/assembler-package2 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/assembler-package2 --parallel 4
ctest --test-dir build/assembler-package2 --output-on-failure -j 4

cmake -S . -B build/assembler-package2-core -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSICXE_BUILD_QT=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build/assembler-package2-core --parallel 4
ctest --test-dir build/assembler-package2-core --output-on-failure -j 4
```

Resultado inicial: **345 testes com Qt** e **342 sem Qt**, todos aprovados,
nenhum ignorado. Na revisão do PR #26, a integração de `SymbolTable` ao catálogo
recebeu três testes adicionais: todos os 38 mnemônicos em diferentes caixas
pelas interfaces de símbolos e valores, diretivas separadas, nomes inválidos
e identificadores não reservados. Os testes também verificam que falhas não
inserem definições ou referências.

Após essa integração, builds limpos Release no Fedora com GCC 15.3.1 passaram
em **348 testes com Qt** e **345 sem Qt**. A fonte GoogleTest existente foi
reutilizada via `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`; não houve instalação de
dependências. Formatação dos arquivos alterados e `git diff --check` passaram.
A CI e a revisão estão no [PR #26](https://github.com/phgiacomelli/sicxe-simulator/pull/26).

Para rodar só os testes novos após o build:

```bash
ctest --test-dir build/assembler-package2 --output-on-failure \
  -R 'InstructionCatalog|InstructionOperand|Format2|SymbolTableTest'
```

As interfaces e a conversão dos diagnósticos estão em [ISA](assembler/isa.md).
