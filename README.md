# SIC/XE Simulator

Simulador da arquitetura SIC/XE desenvolvido em C++20 para a disciplina de
Programação de Sistemas. O projeto reúne uma máquina virtual com interface
gráfica, um montador e um processador de macros.

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

Integrado ao montador pelo executável `SICXEMacroAssembler`. Processa macros
em uma passagem antes da montagem, com parâmetros posicionais, rótulo formal,
redefinições, definições/chamadas aninhadas e rótulos locais `.SER`.
Gera `MASMAPRG.ASM` e entrega exatamente esse texto ao montador existente,
que produz `.bin` e `.lst`. A VM continua recebendo somente bytes.

Não há argumentos por palavra-chave/defaults, expressões de tempo de macro,
linker, loader de objeto ou relocação. `SICXEAssembler` continua disponível
para montagem direta, sem expansão.

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

## Montar um programa com macros

```bash
./build/SICXEMacroAssembler tests/macros/fixtures/nested-sum.asm \
  -o build/nested-sum.bin -l build/nested-sum.lst
```

Esse exemplo define uma macro dentro de outra, faz uma chamada aninhada e
usa `.SER` no laço. Soma 1 a 5, dobra e acrescenta 7. O binário tem 4160
bytes; carregue em `100`, com PC inicial `100` (hexadecimal). Após 28 passos,
A e a palavra em `12C` valem `000025` hexadecimal, ou 37 decimal. Pause no
laço final. A expansão esperada está em
[`nested-sum-expanded.asm`](tests/macros/fixtures/nested-sum-expanded.asm).

Uso: `SICXEMacroAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]`. Aceita
também `--output`, `--listing`, `--` e `--help`/`-h`. Sem opções de saída,
substitui a extensão da **fonte original** por `.bin` e `.lst` no diretório
dela. Retorna `0` em sucesso/ajuda, `1` em erro de processamento/I/O e `2`
em argumentos inválidos.

**Atenção à fonte expandida:** `MASMAPRG.ASM` é publicado no diretório de
trabalho atual, não necessariamente junto da entrada. O nome é fixo e o
conteúdo anterior é substituído após gravação completa em arquivo temporário
e troca atômica. Não use esse caminho como fonte ou saída binária/listagem,
inclusive por aliases. Links simbólicos e destinos não regulares nesse
caminho são recusados. Outros arquivos `.asm` não são ignorados pelo Git.

Falhas de leitura, expansão ou publicação preservam a versão anterior de
`MASMAPRG.ASM`. Após publicação bem-sucedida, a nova versão permanece mesmo
se a montagem ou geração de `.bin`/`.lst` falhar. Binário/listagem nunca são
sobrescritos e seguem a política de diretórios/hard links da montagem direta.
Não há transação única dos três arquivos nem garantia contra desligamento
abrupto ou interferência externa durante as operações de publicação.

Erros de assembly identificam linha/coluna em `MASMAPRG.ASM`, seguidos pela
origem na fonte original e cadeia de chamadas, quando disponíveis. A origem
é por linha: a coluna pode mudar após substituições. Erros de macro apontam
diretamente à fonte original. Nenhum erro de montagem publica binário/listagem
parciais de sucesso.

## Testes

Depois de compilar, execute:

```bash
ctest --test-dir build --output-on-failure
```

Se usou a configuração sem Qt, substitua `build` por `build-core`. Os testes
cobrem a máquina virtual, o montador, o motor de macros e a integração final,
incluindo os dois executáveis de montagem. Na configuração com Qt, há testes do
controlador e da interface, que rodam sem abrir uma janela.

## Contribuir

As orientações para trabalhar no código e verificar as mudanças estão em
[CONTRIBUTING.md](CONTRIBUTING.md).
