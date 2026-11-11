# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido em C++20 para a disciplina de
Programação de Sistemas. O projeto reúne uma máquina virtual com interface
gráfica, um montador e um processador de macros em desenvolvimento.

## Etapas do projeto

### 1. Máquina virtual

Concluída. A máquina virtual simula a memória, os registradores e a execução
das 38 instruções suportadas pelo projeto, incluindo os modos de
endereçamento. A interface em Qt permite carregar programas, executar uma
instrução por vez ou continuamente e acompanhar os registradores e a memória.

A entrada é código de máquina, em texto hexadecimal ou arquivo binário.

### 2. Montador

Concluído. O montador transforma código assembly em um arquivo binário
(`.bin`) e uma listagem com os endereços e bytes gerados (`.lst`). Trabalha
em duas passagens: a primeira calcula os endereços e registra os símbolos;
a segunda resolve as referências e gera o código de máquina.

São aceitas instruções dos formatos 2, 3 e 4, além das diretivas `START`,
`END`, `BASE`, `NOBASE`, `BYTE`, `WORD`, `RESB` e `RESW`. O binário usa
endereços fixos e pode ser carregado na máquina virtual.

### 3. Processador de macros

Em desenvolvimento. Já foram implementadas a leitura do código-fonte, a
captura e o armazenamento das definições, a associação dos argumentos aos
parâmetros e a substituição de parâmetros, respeitando definições aninhadas.

Ainda faltam os rótulos locais, o motor de expansão e a integração com o
montador. Por enquanto, não há um executável para processar macros.

## Compilar e executar

É necessário ter um compilador com suporte a C++20, CMake 3.21 ou superior
e Qt 6 com os módulos Core e Widgets. Os testes também usam Qt Test e
GoogleTest. Se o GoogleTest não estiver instalado, o CMake tenta baixá-lo
durante a configuração.

```bash
cmake -S . -B build
cmake --build build --parallel
./build/SICXESimulator
```

Na interface, use **Carregar Programa** para abrir um binário ou inserir
bytes em hexadecimal. Informe o endereço de carga e o PC inicial, também
em hexadecimal. Use **Executar Passo** para avançar uma instrução ou
**Executar** para execução contínua. **Pausar** interrompe a execução e
permite carregar outro programa ou reiniciar a máquina.

A VM não possui instrução `HALT` nem para automaticamente ao chegar ao fim
dos bytes carregados. Em programas com um laço final, use **Pausar**.

## Montar um programa

A compilação também gera o executável `SICXEAssembler`. Para montar o
exemplo incluído no projeto:

```bash
./build/SICXEAssembler tests/assembler/fixtures/sum-and-table.asm \
  -o build/sum-and-table.bin -l build/sum-and-table.lst
```

Os arquivos de saída não podem existir antes da execução. Os diretórios de
saída precisam existir e permitir a criação de hard links.

Para executar esse exemplo, carregue o binário na interface com endereço
de carga e PC inicial `100`, em hexadecimal. Após 28 passos, o registrador
A e a palavra no endereço `12C` contêm o resultado 37. O programa termina
em um laço, portanto pause a execução nesse ponto.

Para compilar sem a interface gráfica e sem depender de Qt:

```bash
cmake -S . -B build-core -DSICXE_BUILD_QT=OFF
cmake --build build-core --parallel
```

## Testes

Depois de compilar, execute:

```bash
ctest --test-dir build --output-on-failure
```

Se usou a configuração sem Qt, substitua `build` por `build-core`. Os testes
cobrem a máquina virtual, o montador e as partes já implementadas do
processador de macros. Na configuração com Qt, também há testes do
controlador e da interface, que rodam sem abrir uma janela.

## Contribuir

As orientações para trabalhar no código e verificar as mudanças estão em
[CONTRIBUTING.md](CONTRIBUTING.md).
