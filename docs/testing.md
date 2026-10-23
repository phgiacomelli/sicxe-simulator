# Testes

## Ferramentas

Os testes usam GoogleTest e QTest e são registrados no CTest pelo CMake.

A CI verifica formatação e executa **um único build Release com Qt** e toda
a suíte registrada nessa configuração, incluindo núcleo, montador, macros e
testes Qt. Não há segundo build/testes com `SICXE_BUILD_QT=OFF` no workflow.
A opção sem Qt continua disponível para validação local; os resultados com e
sem Qt registrados abaixo são históricos das respectivas entregas.

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
[program-input.md](vm/program-input.md#build-e-integração).

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
O [roteiro de demonstração](vm/demo.md) reproduz o fluxo manual.

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
[program-input.md](vm/program-input.md#build-e-integração); ela também compila
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

## Montador: pacote 4

`tests/assembler/PassOneTest.cpp` exercita a passagem 1 e o contador de
localização no alvo `AssemblerTests`:

- endereços e tamanhos sequenciais para instruções de formatos 2, 3 e 4 (`+`) e `RSUB`;
- referências futuras preservadas na `SymbolTable` sem alterar o tamanho fixado na passagem 1;
- `START` presente/ausente, rótulo inicial de programa e `END` com ou sem rótulo de entrada (`initialPc`);
- preservação de `BASE`, `NOBASE` e linhas de comentário na ordem original sem incrementar o `LOCCTR`;
- diagnósticos de símbolo duplicado (com `previousDefinition`), símbolo indefinido e colisão com nome reservado;
- rejeição de instruções fora do recorte (`ADDF`, `HALT`, `EQU`, `ORG`), `+` inválido, falta de `END`, declarações após `END` e `START`/`END` duplicados ou fora de lugar;
- validação do último byte endereçável (`0xFFFFF`), próximo endereço inválido (`0x100000`) e overflow sem *wrap-around*;
- integração de `BYTE`, `WORD`, `RESB` e `RESW`, com tamanhos estáveis e rótulos seguintes no endereço correto (pacote 5);
- recusa de montagem parcial quando `SourceParser::parse` reporta falha sintática e reconhecimento das 38 instruções da VM.

São 11 testes novos de passagem 1. Na revisão do PR #27, o teste das diretivas
não integradas foi reforçado para confirmar ausência de símbolo/linha com tamanho
desconhecido e de endereços para instruções posteriores. Esse teste falhou antes
da correção e passou depois. Builds limpos Release no Fedora passaram em
**356 testes sem Qt** e **359 com Qt**, com formatação e `git diff --check`
aprovados. A fonte GoogleTest existente foi reutilizada por
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, sem instalação de dependências.

Para executar os testes do pacote 4 após o build:

```bash
ctest --test-dir build -R 'PassOneTest' --output-on-failure
```

O contrato do intermediário `LocatedLine` e os códigos de diagnóstico da
passagem 1 estão em [passagem 1 do montador](assembler/pass-one.md).

## Montador: pacote 5

`tests/assembler/DataDirectiveTest.cpp` exercita `measureData` e `encodeData`
pela interface pública e integra as diretivas à `PassOne` real:

- ASCII com preservação de caixa, espaços, pontuação, NUL e `7F`; rejeição de
  bytes não ASCII; hexadecimal com pares, caixa mista, dígito inválido e ímpar;
- literais vazios e tamanhos máximos, sem emissão na medição;
- `WORD` com limites de 24 bits, negativos em complemento de dois, overflow,
  símbolos anteriores/futuros, autorreferência e indefinidos;
- reservas zero, normais e máximas sem buffer de bytes; rejeição de rótulos,
  expressões, negativos e multiplicação que excederia a capacidade;
- consistência entre tamanho medido e codificado, detecção de intermediário
  divergente e preservação da fronteira do último byte endereçável;
- erros de diretiva impedem rótulos/endereço fictício nas linhas posteriores;
  duas montagens sucessivas não compartilham símbolos.

```bash
ctest --test-dir build -R 'DataDirectiveTest|PassOneTest' --output-on-failure
```

Contratos, diagnósticos e escolhas estão em [dados e reservas](assembler/data.md).

Na revisão do PR #28, builds limpos Release no Fedora confirmaram **370 testes
sem Qt** e **373 com Qt**, além da formatação dos C++ alterados e
`git diff --check`. A fonte GoogleTest existente foi reutilizada via
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, sem instalar dependências. Parecer e CI
do commit final ficam registrados no PR.

## Montador: pacote 6

`InstructionEncoderTest.cpp` roda em `AssemblerTests`, sem dependência da VM.
Cobre bytes esperados manualmente para todos os modos, formato 2 delegado,
`RSUB`, limites de campos/deslocamentos, prioridade PC/base, base zero ou
desativada, erros encapsulados, intermediário inconsistente e ausência de estado
entre chamadas.

`InstructionEncoderIntegrationTest.cpp` roda em `AssemblerDecoderTests`, com
`SICXECore`, inclusive na configuração sem Qt. Cobre os 38 opcodes pela API do
encoder e 540 combinações de modos/estratégias nas 27 instruções de memória,
conferindo decoder e resolução efetiva. Integra os layouts de `PassOne` e
`DataDirective`, testa dois programas na `Machine` somente por bytes e preserva
a restrição existente de PC pós-fetch no fim da memória.

```bash
ctest --test-dir build -R 'InstructionEncoder' --output-on-failure
```

As prioridades, diagnósticos e limites estão em [codificação](assembler/encoding.md).

Na revisão do PR #29, builds limpos Release no Fedora confirmaram **385 testes
sem Qt** e **388 com Qt**, incluindo os 15 testes novos, as 540 combinações
de endereçamento e os dois programas executados pela VM. Formatação dos C++
alterados e `git diff --check` passaram. A fonte GoogleTest existente foi
reutilizada por `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, sem instalação de
dependências. O ajuste de revisão reutiliza a constante compartilhada do campo
de 12 bits; não altera bytes, formatos ou prioridades de endereçamento.

## Montador: pacote 7

O pacote final acrescenta **26 testes**, sem alterar a VM:

- 11 em `AssemblerTest.cpp` (`AssemblerTests`): pipeline real, bytes esperados
  à mão, listagem exata, símbolos/referências futuras, dados/reservas, ordem de
  `BASE`/`NOBASE`, base zero e independência entre chamadas. Exercitam erros
  sintáticos/semânticos/tardios sem sucesso parcial, programa vazio, último
  byte, reserva de 1 MiB e layout inconsistente pelas APIs públicas.
- 11 em `AssemblerCliTest.cpp` (`AssemblerCliTests`): leitura e gravação em
  diretórios temporários reais, caminhos com espaços e em diretórios distintos,
  bytes `00`/`80`/`FF`/`0A`/`0D`, ajuda/argumentos/retornos, diagnóstico com
  arquivo/linha/coluna, falhas de fonte/destino, preservação da entrada e das
  saídas existentes, destinos iguais e link simbólico pendente. O caso de link
  pode ser ignorado em plataformas que não permitem sua criação; no Fedora
  local ele passou normalmente.
- Dois em `AssemblerAcceptanceTest.cpp` (`AssemblerDecoderTests`): as 38
  instruções montadas e decodificadas pelo pipeline completo; e a fixture
  [sum-and-table.asm](../tests/assembler/fixtures/sum-and-table.asm), comparada
  com 4160 bytes calculados manualmente e executada por 28 passos na `Machine`.
  Compara registradores e memória completa: `SUM = 15`, `RESULT = A = 37`,
  B=`0x1138`, PC=`0x126`. A máquina recebe apenas os bytes e endereços.
- `AssemblerCliHelp` e `AssemblerCliSmoke` invocam o executável real. O smoke
  usa nomes com espaços, verifica binário completo (prefixo/reserva/sufixo),
  listagem/carga e recusa de sobrescrita ao repetir a chamada. Seus arquivos
  gerados ficam em um subdiretório próprio do build; não são versionados.

Validação local no Fedora, em diretórios novos, sem instalar dependências:

```bash
cmake -S . -B build/assembler-package7 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DSICXE_BUILD_QT=OFF
cmake --build build/assembler-package7 --parallel 4
ctest --test-dir build/assembler-package7 --output-on-failure --parallel 4

cmake -S . -B build/assembler-package7-qt -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSICXE_BUILD_QT=ON
cmake --build build/assembler-package7-qt --parallel 4
ctest --test-dir build/assembler-package7-qt --output-on-failure --parallel 4
```

A fonte GoogleTest já disponível em `build/_deps/googletest-src` foi reutilizada
nas duas configurações por `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` (caminho absoluto
local); os comandos acima omitem esse ajuste opcional de cache. Resultado:
**411 testes sem Qt e 414 com Qt**, todos aprovados, nenhum ignorado. A interface
Qt foi testada em `offscreen` pelo CTest. `clang-format --dry-run --Werror` em
todos os C++ do projeto e `-Wall -Wextra -Wpedantic -Werror` nos novos fontes
também passaram. Na revisão técnica do
[PR #30](https://github.com/phgiacomelli/sicxe-simulator/pull/30), builds novos
em Release repetiram os **411 testes sem Qt e 414 com Qt**, todos aprovados.
O job remoto `Format, build and test` também passou. A revisão não encontrou
bloqueadores de padrões ou de escopo no pipeline e na publicação dos arquivos.

Também foi compilado o alvo `SICXEAssembler` em outro build Release, com
`SICXE_BUILD_QT=OFF` e `BUILD_TESTING=OFF`, sem Qt/GoogleTest. Esse executável
montou a demonstração e gerou `.bin` de 4160 bytes e `.lst`, informando carga
e entrada `0x000100`.

Para executar apenas os casos do pacote 7, depois do build:

```bash
ctest --test-dir build --output-on-failure \
  -R 'AssemblerTest|AssemblerCli|AssemblerAcceptanceTest'
```

O [guia de uso](assembler/usage.md) documenta a montagem, a política de arquivos
sem sobrescrita, a exigência de hard links e os valores esperados na interface.

## Macros: pacote 1

O pacote 1 acrescentou ao alvo `MacroTests` **14 testes**, originalmente sem
dependência de Qt, VM ou montador e sem mocks:

- 12 em `tests/macros/SourceScannerTest.cpp`, pela interface
  `macro::SourceScanner::scan`: campos textuais, posições físicas de byte,
  LF/CRLF/CR, ausência de linha fictícia após terminador final, comentários
  inteiros/inline, separadores entre aspas e reconhecimento de `MCDEFN`/`MCEND`
  somente como operação completa. Verificam cópia LF sem expansão, campos de
  assembly inválidos preservados, `.SER`/parâmetros/marcadores SIC/XE, aspa
  aberta sem estado na próxima linha, tabs/UTF-8, NUL e duração dos dados.
- Dois em `tests/macros/MacroContractTest.cpp`: valores públicos de sucesso
  com texto/origens/cadeia aninhada e falha só com diagnósticos. Modificam e
  destroem as strings de entrada para conferir propriedade dos dados. Uma
  asserção de compilação, em expressão não avaliada, confere a declaração de
  `expand()` sem exigir uma implementação antecipada ou stub.

Esses testes não validam captura, argumentos, substituição, serial, motor ou
arquivos de macro: essas responsabilidades são dos pacotes 2–7.
`normalizedText()` é cópia lexical; seu teste confirma que diretivas e
comentários estruturais continuam presentes, não que houve expansão.

Validação local no Fedora, em diretórios novos:

```bash
cmake -S . -B build/macros-package1-core -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DSICXE_BUILD_QT=OFF
cmake --build build/macros-package1-core --parallel 4
ctest --test-dir build/macros-package1-core --output-on-failure --parallel 4

cmake -S . -B build/macros-package1-qt -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSICXE_BUILD_QT=ON
cmake --build build/macros-package1-qt --parallel 4
ctest --test-dir build/macros-package1-qt --output-on-failure --parallel 4
```

GoogleTest já disponível em `build/_deps/googletest-src` foi reutilizado nas
duas configurações por `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, com caminho
absoluto local, sem instalação de dependências. Os comandos acima omitem
esse ajuste opcional de cache.

Resultado: **425 testes sem Qt e 428 com Qt**, todos aprovados, nenhum
ignorado. Incluem os testes existentes do montador, núcleo, adaptador e
interface, esta em `offscreen` pelo CTest na configuração com Qt.
Formatação de todos os C++ do projeto e compilação dos dois novos `.cpp`
com `-Wall -Wextra -Wpedantic -Werror` passaram. Cada novo header também
compilou isoladamente, incluído por um arquivo de entrada mínimo.

A biblioteca foi compilada separadamente com `BUILD_TESTING=OFF` e
`SICXE_BUILD_QT=OFF`, pelo alvo `sicxe_macros`, sem Qt/GoogleTest ou compilação
dos alvos do montador/VM. A revisão técnica do
[PR #31](https://github.com/phgiacomelli/sicxe-simulator/pull/31) repetiu os
**425 testes sem Qt e 428 com Qt** em builds Release novos, todos aprovados.
O job remoto `Format, build and test` também passou. Não foram encontrados
bloqueadores de padrões ou de requisitos no pacote 1.

Para rodar somente os casos do pacote depois do build:

```bash
ctest --test-dir build/macros-package1-core --output-on-failure \
  -R 'SourceScannerTest|MacroContractTest'
```

As interfaces e limites atuais estão em
[contratos de macros](macros/contracts.md#interfaces-c-entregues-no-pacote-1).

## Macros: pacote 2

O alvo `MacroTests` acrescenta **21 testes**, 17 em `DefinitionCaptureTest.cpp`
e quatro em `MacroTableTest.cpp`, pelas interfaces de captura e tabela:

- Definição simples, vazia e com três níveis; fechamento correto e retorno
  do próximo índice, sem consumir linhas posteriores.
- Protótipo na próxima linha ativa, comentários estruturais guardados como
  metadados, comentários/linhas vazias comuns e preservação do esqueleto
  interno completo, texto, campos e posições físicas.
- Falhas por código e posição: delimitador isolado, falta de protótipo,
  nome inválido/reservado, campos indevidos nos delimitadores e falta de
  fechamento. Formais e assembly temporariamente inválidos são preservados.
- Consulta sem distinção de caixa, redefinições maiores/menores/vazias,
  versões anteriores imutáveis e válidas após destruição da tabela, tabelas
  independentes e ausência de instalação antecipada das definições internas.

O módulo liga privadamente `sicxe_assembler` para reutilizar apenas as funções
de nomes de `SymbolTable`. Os testes continuam sem Qt ou VM, inclusive no
build com Qt. Não chamam `expand()` nem simulam expansão, montagem ou I/O.

Comandos usados em diretórios novos, com Unix Makefiles (Ninja indisponível):

```bash
cmake -S . -B build/macros-package2-noqt -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug -DSICXE_BUILD_QT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build/macros-package2-noqt --target format
cmake --build build/macros-package2-noqt --parallel 4
ctest --test-dir build/macros-package2-noqt --output-on-failure --parallel 4

cmake -S . -B build/macros-package2-qt -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DSICXE_BUILD_QT=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build/macros-package2-qt --parallel 4
ctest --test-dir build/macros-package2-qt --output-on-failure --parallel 4
```

GoogleTest existente em `build/_deps` foi reutilizado sem download/instalação.
Resultado: **446 testes sem Qt (Debug) e 449 com Qt (Release)**, todos
aprovados, nenhum ignorado. Os 21 novos casos passaram nas duas configurações.
Novos fontes e headers isolados passaram com
`-std=c++20 -Wall -Wextra -Wpedantic -Werror`. A formatação de todos os C++
e `git diff --check` passaram. Resultados locais não representam revisão
de outro integrante ou aprovação de CI remota.

Para executar apenas os casos deste pacote depois do build:

```bash
ctest --test-dir build/macros-package2-noqt --output-on-failure \
  -R 'DefinitionCaptureTest|MacroTableTest'
```

Handoff e diagnósticos estão no
[contrato do pacote 2](macros/contracts.md#interfaces-internas-entregues-no-pacote-2).
