# Entrada de programas e adaptador Qt

## Formatos e fronteira

`ProgramInput` contém `bytes`, `loadAddress` e `initialPc` (endereços sem sinal
de 32 bits). `HexParser::parse` e `RawBinaryReader::read` devolvem
`ProgramInputResult`, uma variante de `ProgramInput` ou `InputFault`. São C++20
puro, sem Qt. Nenhum interpreta assembly, formato objeto, símbolos ou relocação.

O hexadecimal aceita exatamente dois dígitos ASCII por byte, separados por um
ou mais espaços, tabs, CR, LF, form feed ou vertical tab. Maiúsculas e minúsculas
podem ser misturadas. Espaços nas extremidades são opcionais. Por exemplo:

```cpp
auto input = HexParser::parse("01 00 05\n19 00 01", 0x100, 0x100);
controller.loadProgram(input);
```

São os bytes de `LDA #5; ADD #1` já codificados; o parser não interpreta esses
mnemônicos. Prefixos `0x`, vírgulas, comentários e sequências concatenadas não
são aceitos. Tokens como `000` também são rejeitados: cada token representa
exatamente um byte, mesmo quando zeros à esquerda não mudariam o valor.

O leitor abre arquivos com `std::ios::binary`, preservando `00..FF`, inclusive
NUL, CR e LF. A sobrecarga de stream permite testar falhas de leitura; o chamador
deve fornecer um stream binário quando isso for relevante à plataforma.

```cpp
auto input = RawBinaryReader::read("program.bin", 0x100, 0x100);
controller.loadProgram(input);
```

## Falhas

| Código de entrada | Significado |
| --- | --- |
| `EmptyInput` | Texto sem bytes ou arquivo vazio |
| `InvalidDigit` | Caractere que não é dígito hexadecimal dentro de um token |
| `IncompleteByte` | Token de um dígito |
| `ByteOutOfRange` | Token com mais de dois dígitos |
| `OpenFailed` | Não foi possível abrir o arquivo |
| `ReadFailed` | Falha ao ler o stream; nenhum programa parcial é entregue |

Falhas de texto incluem `offset` em bytes a partir de zero. Dígito inválido
aponta para o caractere; falhas de tamanho apontam para o início do token.
Caracteres são validados antes do tamanho do token. Entrada vazia e erros de
arquivo não têm offset. Falhas de alocação não são convertidas em erro de sintaxe.

Os parsers preservam os endereços recebidos. A `Machine` valida carga, extensão
do programa e PC, sem duplicação de regras no adaptador. Os `LoadFault` e
`MachineFault` seguem integralmente o [contrato do pacote 4](machine-cycle.md).
Entrada inválida não altera memória, registradores, última falha nem estado de
execução contínua. Se já estiver rodando, a sessão continua executando normalmente.

## Controller e sinais

`MachineController` possui sua `Machine` privada. A UI não recebe referências
mutáveis à máquina nem executa operações diretamente no núcleo.

Todos os comandos são enfileirados com `Qt::QueuedConnection`, mesmo quando
chamados no próprio thread. O event loop do thread proprietário deve estar ativo.
Comandos de um mesmo emissor são processados na ordem de envio. A UI deve aguardar
os sinais, não presumir conclusão no retorno do método. Como qualquer `QObject`,
o controller deve ser destruído no thread proprietário, sem chamadas concorrentes
após a destruição. Não executar loops de eventos aninhados nos consumidores dos sinais.

| Comando | Resultado observável |
| --- | --- |
| `loadProgram(ProgramInputResult)` | `inputRejected(InputFault)` para parsing inválido; caso contrário `loadFinished(LoadResult)` e snapshot. Carga válida pausa e substitui a sessão. |
| `reset()` | Pausa, zera a máquina conforme o pacote 4, emite `resetFinished()` e snapshot. |
| `step()` | Pausa e executa exatamente um passo, emite `stepFinished(StepResult)` e snapshot. |
| `run()` | Inicia timer; chamadas repetidas são idempotentes. |
| `pause()` | Para o timer na próxima fronteira entre instruções; é idempotente. |
| `requestSnapshot()` | Publica o snapshot atual sem executar instrução. |

`runningChanged(bool)` só é emitido quando o estado de execução muda.
`stepFinished` preserva instrução, efeitos e PC no sucesso, ou todos os detalhes
da falha do núcleo. Uma falha de execução para o timer antes de publicar resultado
e snapshot. Não há HALT sintético, limite de passos ou parada implícita no fim dos
bytes carregados; o usuário pausa, ou uma falha encerra a execução contínua.

`snapshotChanged(ImmutableMachineSnapshot)` entrega um
`std::shared_ptr<const MachineSnapshot>`: memória, registradores e última falha
pertencem a uma cópia imutável, que pode ser retida após comandos posteriores.
Tipos dos sinais são registrados no sistema de metatypes Qt, inclusive para
conexões enfileiradas. A UI decide como apresentar erros, sem conversão precoce
para strings no adaptador.

## Responsividade e decisões

A única estratégia é um `QTimer` de 1 ms no thread do controller, com uma instrução
por disparo. Cada callback termina antes do próximo comando ou instrução, permitindo
que o event loop processe pintura, entrada e pausa. Comandos disparados pelos
próprios sinais também ficam enfileirados, evitando reentrada síncrona na máquina.

Um worker thread foi considerado, mas adicionaria sincronização e gestão de vida
sem necessidade neste incremento. O intervalo positivo evita um timer de zero
ocupando continuamente o event loop. Não é uma garantia de frequência nem um
simulador de tempo real. Snapshots copiam toda a memória por passo; prioriza-se
simplicidade e observabilidade, com custo de throughput. A leitura/parsing de
arquivos é síncrona e deve terminar antes da carga; a garantia de responsividade
se refere à execução contínua, não a arquivos arbitrariamente grandes.

## Build e integração

`SICXECore` contém somente C++20; `sicxe_input` também não depende de Qt.
`sicxe_application` liga esses módulos a `Qt6::Core`; widgets pertencem à aplicação
visual. Para verificar o núcleo e parsers sem localizar ou vincular Qt:

```bash
cmake -S . -B build/core-only -DSICXE_BUILD_QT=OFF
cmake --build build/core-only
ctest --test-dir build/core-only --output-on-failure
```

Com `BUILD_TESTING=OFF`, GoogleTest e Qt Test não são necessários. Na configuração
padrão os testes do controller usam QTest e `QCoreApplication`, sem display.
O pacote 6 pode conectar os comandos e sinais aos widgets; layout fica fora deste
pacote. Nenhuma interface pública do pacote 4 foi alterada.
