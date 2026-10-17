# Arquitetura da VM SIC/XE

## Objetivo

Este documento apresenta a organização do simulador, a responsabilidade de
cada módulo e o fluxo de execução de uma instrução.

A VM executa programas SIC/XE já codificados. Tradução de assembly não
pertence **à VM**: é responsabilidade do montador de duas passagens planejado
para a [segunda etapa](assembler/README.md). Linking, formato objeto para
loader e relocação durante a carga não pertencem a nenhuma das duas etapas.

## Decisões de interpretação do enunciado

- O enunciado original menciona Java, mas o professor posteriormente deixou a
  escolha da linguagem ao grupo. O grupo escolheu C++20 e Qt 6; a referência a
  Java no PDF é legada, não um requisito vigente. Novos pacotes devem preservar
  essa base e não introduzir um segundo runtime.
- A memória é endereçada por byte: cada endereço identifica um byte e uma
  palavra ocupa os endereços `m`, `m+1` e `m+2`. Essa decisão segue as ações das
  instruções e os modos de endereçamento do próprio enunciado, apesar de sua
  tabela descrever a unidade de endereçamento como “palavra”.
- Somente as 38 instruções não destacadas em vermelho são conhecidas pela VM.
  As demais não são cadastradas como instruções parcialmente suportadas.

## Fronteira entre VM e montador

O ponto de entrada da VM é uma sequência de bytes de código de máquina já
codificados. Esses bytes podem ser fornecidos diretamente por um arquivo binário
bruto ou escritos em representação hexadecimal; as duas formas representam a
mesma entrada para a máquina.

```text
assembly -- responsabilidade do montador --> bytes codificados + listagem
bytes codificados -- responsabilidade da VM --> estado da máquina
```

A VM pode carregar os bytes na memória, decodificá-los e executá-los. Ela não
deve interpretar código-fonte assembly, mnemônicos, diretivas, labels, tabelas de
símbolos ou registros de formato objeto, nem realizar linking ou relocação.

Neste projeto, **arquivo binário bruto** significa apenas uma sequência de bytes
copiada para a memória. Esse termo não é sinônimo de arquivo objeto, que poderia
conter registros e informações a serem interpretados por um loader.

O montador planejado exporta esse binário **para o endereço de carga
declarado** e uma listagem. Não produz objeto textual `H/T/M/E`; a VM
não lê registros de objeto nem aplica relocação. Veja os
[contratos da segunda etapa](assembler/contracts.md).

## Visão geral

O fluxo completo é:

```text
Memory
  ↓ bytes da instrução
Decoder
  ↓ DecodedInstruction
AddressResolver
  ↓ operando ou endereço resolvido
Executor
  ↓ alterações calculadas
Machine
  ↓ estado confirmado
Interface Qt
```

Cada estágio tem uma responsabilidade específica. Em particular, decodificar,
resolver um endereço e executar uma operação são atividades diferentes.

## Módulos

### `Memory`

Implementada em `src/core/Memory.*`.

Responsabilidades atuais:

- armazenar bytes;
- ler e escrever um byte;
- ler e escrever palavras de 24 bits;
- armazenar palavras em ordem big-endian;
- rejeitar acessos que ultrapassem o espaço alocado;
- conservar apenas os 24 bits menos significativos de uma palavra escrita.

Uma palavra ocupa três bytes consecutivos. Por exemplo, `0x123456` é armazenada
como `0x12`, `0x34` e `0x56`, nessa ordem.

### `RegisterFile`

Implementado em `src/core/RegisterFile.*`.

Mantém os registradores `A`, `X`, `L`, `B`, `S`, `T`, `F`, `PC` e `SW`. Escritas
nos registradores inteiros são limitadas a 24 bits; `F` é limitado a 48 bits.

Os registradores inteiros aceitos pelo decoder possuem identificação tipada e
acesso centralizado. O Condition Code ocupa os dois bits inferiores de `SW` e
pode ser lido ou atualizado preservando os demais bits. O arquivo de
registradores possui testes próprios para reset, máscaras, acesso tipado e CC.

### `Decoder`

Implementado em `src/core/Decoder.*`, com os tipos compartilhados em
`src/core/Instruction.hpp`.

O decoder:

- lê até quatro bytes a partir de um endereço;
- reconhece as 38 instruções pertencentes ao projeto;
- identifica os formatos SIC, 2, 3 e 4;
- separa registradores, flags e o campo de operando codificado;
- valida combinações de flags e campos reservados;
- devolve uma instrução decodificada ou uma falha estruturada.

Ele não calcula endereços efetivos, não lê o valor final de operandos e não
altera o estado da máquina. As regras completas estão em [decoder.md](decoder.md).

### `AddressResolver`

Implementado em `src/core/AddressResolver.*`, com os tipos compartilhados em
`src/core/Architecture.hpp` e `src/core/Operand.hpp`.

Transforma os campos entregues pelo decoder em operandos tipados utilizáveis
pelo executor. Isso inclui:

- endereçamento SIC;
- direto;
- relativo ao PC;
- relativo à base;
- formato 4;
- indexação por `X`;
- imediato;
- indireto.

O resolver calcula e valida sem executar instruções ou alterar memória,
registradores e PC. As regras, tipos e falhas estão detalhados em
[addressing.md](addressing.md).

### `Executor`

Implementado em `src/core/Executor.*`. O dispatcher seleciona a família pelo
opcode e delega às três famílias de operações: `src/core/execution/DataOperations.*`
(18 operações de transferência de dados e lógica),
`src/core/execution/ArithmeticOperations.*` (14 operações aritméticas,
relacionais, de shift e indexação) e `src/core/execution/ControlOperations.*`
(`J`, `JEQ`, `JGT`, `JLT`, `JSUB` e `RSUB`). Essas operações recebem uma
instrução decodificada e um operando já resolvido e produzem um `ExecutionEffect`
ou um `ExecuteFault` sem modificar diretamente a máquina.

A lógica de endereçamento não deverá ser repetida dentro das operações.
Os contratos compartilhados estão definidos em `src/core/ExecutionEffect.hpp` e
documentados em [execution-contracts.md](execution-contracts.md).

### `Machine`

Implementada em `src/core/Machine.*`, com os tipos de falha em
`src/core/MachineFault.hpp` e a validação de efeitos em
`src/core/EffectValidation.*`. Oferece `loadProgram`, `reset`, `step` e
`snapshot`, e coordena o ciclo:

1. obter o PC atual;
2. decodificar a instrução;
3. calcular o PC posterior ao fetch;
4. resolver o operando;
5. executar a operação;
6. validar todas as alterações;
7. confirmar memória, registradores e PC.

A confirmação ocorre somente depois que a instrução inteira tiver sido
validada. Assim, uma falha não deixa alterações parciais no estado. As etapas,
as falhas, a atomicidade e as decisões do ciclo estão em
[machine-cycle.md](machine-cycle.md).

### Adaptador da aplicação

Implementado em `src/application/`: os parsers C++20 produzem `ProgramInput`,
e o `MachineController` Qt comanda a interface pública da `Machine`, publicando
snapshots imutáveis e resultados tipados. A execução contínua usa um único
`QTimer`, sem dependência de Qt no núcleo. Contratos e decisões estão em
[program-input.md](program-input.md).

### Interface Qt

Implementada em `src/ui/`. `MainWindow` conecta os widgets ao
`MachineController`; nenhum widget recebe referência mutável à `Machine`.
`ProgramLoadDialog` aceita texto hexadecimal ou arquivo binário bruto e mantém
o diálogo aberto até receber `LoadResult`. Falhas de sintaxe e de carga aparecem
próximas aos campos, sem substituir a sessão anterior.

`ExecutionPanel` controla carga, passo, execução, pausa e reset, e mostra o
estado da sessão e a instrução decodificada no PC atual com seus bytes. Os
demais painéis exibem todos os registradores e Condition Code, uma faixa
consultável de memória e falhas tipadas persistentes. Os destaques do último
passo vêm do efeito confirmado pelo núcleo; não são edições na VM.

O campo `currentInstruction` do snapshot é uma decodificação somente de leitura
no PC capturado. Ele não executa a instrução nem altera o estado; quando o PC
não contém instrução reconhecida, o campo é vazio e o diagnóstico da tentativa
de execução aparece no painel de falhas. Como não existe `HALT` sintético, na
interface a execução contínua termina por pausa ou falha. Enquanto está rodando,
os botões de carga e reset ficam desabilitados; após pausar, o usuário pode
carregar outro programa ou resetar. O `MachineController` também aceita esses
comandos durante execução e interrompe o timer, mas a UI não os oferece nesse
estado.

## Dependências permitidas

As dependências devem seguir esta direção:

```text
UI → MachineController → Machine → Decoder / AddressResolver / Executor → Memory / RegisterFile
```

O núcleo não deve depender de Qt. A interface pode observar e comandar a
máquina, mas não deve modificar diretamente a memória ou os registradores.
