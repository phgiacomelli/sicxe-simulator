# Contratos de execução

## Responsabilidade

As operações do executor calculam alterações sem modificar a VM. O resultado é
um `ExecuteResult`, que contém um `ExecutionEffect` completo ou um
`ExecuteFault`. A `Machine` validará todo o efeito antes de confirmá-lo.

Os tipos estão definidos em `src/core/ExecutionEffect.hpp` e não dependem de
Qt, da interface gráfica ou de formatos de montador.

## `ExecutionEffect`

Todo efeito é criado com um `nextPc` explícito. Para instruções sem desvio, ele
deve receber o PC pós-fetch. Saltos substituem esse valor pelo destino calculado.

Um efeito pode carregar:

- uma escrita de registrador inteiro por `RegisterWrite`;
- uma escrita de memória, diferenciada entre `ByteWrite` e `WordWrite`;
- uma atualização de `ConditionCode`;
- o próximo PC.

Uma escrita de registrador e uma atualização de CC podem coexistir, como em
`TIX` e `TIXR`. `JSUB` usa uma escrita em `L` junto com o destino em `nextPc`.
Valores de registrador e palavra são normalizados para 24 bits na construção.
O efeito armazena somente valores, nunca referências para a máquina.

## Escritas de memória

`ByteWrite` representa uma escrita de um byte. `WordWrite` representa uma
palavra de 24 bits, que a `Machine` confirmará em ordem big-endian. A variante
`MemoryWrite` impede que o consumidor precise inferir a largura pelo opcode.

Os endereços ainda devem ser validados pela `Machine` antes do commit. Uma
palavra precisa possuir três bytes válidos a partir do endereço informado.

## Validação e commit

A `Machine` deve validar o efeito inteiro antes da primeira escrita. No mínimo,
isso inclui o `nextPc`, o identificador de registrador e o intervalo completo
de qualquer escrita de memória. Somente depois dessas validações ela pode
confirmar registrador, memória, Condition Code e PC.

O `Executor` normaliza escritas de formato 2 em `PC` para o campo `nextPc` antes
de devolver o efeito. Escritas em `SW` não podem coexistir com uma atualização
de Condition Code. Motivos, alternativas e consequências dessas decisões estão
registrados somente no [ciclo da máquina](machine-cycle.md#decisões-do-ciclo).

## Falhas

`ExecuteFault` informa:

- o código estruturado da falha;
- o endereço da instrução;
- opcionalmente, o valor que causou a falha.

Os códigos iniciais cobrem opcode não suportado, operando incompatível, divisão
por zero e acesso de memória fora dos limites. Uma falha não carrega nem aplica
um efeito parcial.

## PC pós-fetch

`computePostFetchPc(instructionAddress, instructionLength)` soma o tamanho da
instrução em tipo largo e rejeita tamanho zero ou wrap além dos 24 bits. A
`Machine` ainda deve validar o resultado contra o tamanho real da memória antes
de executar ou confirmar o efeito.

As operações recebem o PC pós-fetch já calculado. Elas não devem repetir o
cálculo usando `instructionAddress` ou `length`.
