# Guia de integração do pacote 2

Este documento complementa o [pacote 2](02-data-operations.md) com os contratos
concretos entregues pelo pacote 1. Ele explica como consumir operandos e
registradores sem mover regras de endereçamento para as operações.

## Antes de implementar

Leia, nesta ordem:

1. [arquitetura da VM](../architecture.md);
2. [resolução de operandos](../addressing.md);
3. [especificação do pacote 2](02-data-operations.md).

O contrato de `ExecutionEffect` e `ExecuteFault` ainda precisa ser aprovado com
os responsáveis pelos pacotes 3 e 4. Estabilize esse contrato antes de escrever
as implementações finais das 18 instruções.

## Entradas disponíveis

`executeDataOperation` receberá:

- `DecodedInstruction`, que identifica o opcode e preserva os campos
  codificados;
- `ResolvedOperand`, que já contém registradores tipados, valor imediato ou
  localização de memória;
- `postFetchPc`, que deve ser o próximo PC quando não há controle de fluxo;
- referências constantes para `Memory` e `RegisterFile`.

O módulo não deve consultar `n`, `i`, `x`, `b`, `p` ou `e`. Toda interpretação
desses campos termina no `AddressResolver`.

## Variantes de `ResolvedOperand`

| Variante | Uso no pacote 2 |
| --- | --- |
| `RegisterOperands` | `CLEAR` e `RMO` |
| `ImmediateValue` | loads de palavra, `LDCH`, `AND` e `OR` |
| `MemoryAddress` | loads, stores, `LDCH`, `STCH`, `AND` e `OR` |
| `NoOperand` | incompatível com todas as instruções do pacote 2 |

Uma variante incompatível deve produzir `ExecuteFault`. Não use exceção nem
tente reinterpretar o operando.

## Acesso aos registradores

Use exclusivamente:

```cpp
std::uint32_t value = registers.read(registerId);
```

para consultar registradores inteiros. Uma futura escrita deve ser descrita no
efeito, usando o mesmo `RegisterId`; não chame `registers.write` dentro das
operações.

`F` não possui `RegisterId` e está fora do conjunto atual. O Condition Code não
é alterado por nenhuma instrução deste pacote.

Nos operandos de formato 2:

- `CLEAR` usa somente `r1`;
- `RMO` lê `r1` e produz a escrita `r2 ← r1`;
- `r2` ausente em `RMO` é falha de execução.

## Matriz das operações

| Família | Origem aceita | Destino | Largura |
| --- | --- | --- | --- |
| `CLEAR` | `RegisterOperands::r1` | registrador `r1` | 24 bits |
| `RMO` | registrador `r1` | registrador `r2` | 24 bits |
| `LDA/LDB/LDL/LDS/LDT/LDX` | imediato ou memória | registrador do opcode | 24 bits |
| `STA/STB/STL/STS/STT/STX` | registrador do opcode | memória | 3 bytes |
| `LDCH` | imediato ou byte de memória | 8 bits inferiores de `A` | 8 bits |
| `STCH` | 8 bits inferiores de `A` | memória | 1 byte |
| `AND/OR` | imediato ou palavra de memória | `A` | 24 bits |

Para loads e lógica, um `ImmediateValue` é o próprio valor. Um
`MemoryAddress` indica onde ler; o tamanho da leitura depende do opcode. Stores
aceitam somente `MemoryAddress`.

## Produção de efeitos

As operações devem ser funções puras. O resultado descreve, sem aplicar:

- o próximo PC, inicialmente igual a `postFetchPc`;
- no máximo as escritas de registrador necessárias;
- escritas de memória com largura explícita de byte ou palavra;
- uma falha, sem efeitos parciais.

Não armazene referências dentro do efeito. Use valores próprios para que a
`Machine` consiga validar todos os destinos antes do commit atômico.

É útil criar dois helpers internos:

1. leitura de valor a partir de `ImmediateValue` ou `MemoryAddress`, recebendo a
   largura esperada;
2. construção de escrita de byte ou palavra após validar a compatibilidade do
   operando.

Esses helpers não devem calcular endereços nem capturar exceções como sucesso.
Converta acessos fora da memória em `ExecuteFault` estruturado.

## Cuidados por instrução

- Loads e stores de palavra usam exatamente 24 bits.
- `Memory::readWord` e `writeWord` representam palavras em big-endian.
- `LDCH` preserva os 16 bits superiores atuais de `A`.
- `STCH` usa somente `A & 0xFF` e não escreve bytes vizinhos.
- `AND` e `OR` operam em padrões unsigned de 24 bits.
- `CLEAR` e `RMO` não acessam memória.
- Nenhuma operação altera `SW` ou Condition Code.
- Não aplique máscaras manualmente em escritas de registrador se o contrato do
  efeito já garantir valores de 24 bits; mantenha uma única regra de
  normalização.

## Estratégia de testes

Organize `DataOperationsTest.cpp` por opcode e inspecione o efeito sem aplicá-lo.
Além dos casos obrigatórios do pacote 2, verifique:

- que memória e registradores permanecem idênticos antes e depois da função;
- que o efeito contém somente o destino esperado;
- que `nextPc` é exatamente `postFetchPc`;
- que `NoOperand`, `RegisterOperands` indevido e imediato em store falham;
- que uma leitura de palavra nos dois últimos bytes da memória falha;
- que `LDCH` e `STCH` funcionam no último endereço válido;
- que padrões com bit de sinal, como `0x800000` e `0xFFFFFF`, são preservados;
- que falhas não carregam escritas parciais.

Não escreva testes por meio da `Machine` neste pacote. Os testes devem chamar a
função pura diretamente; integração e commit pertencem ao pacote 4.

## Arquivos e integração

Crie os arquivos sugeridos em `src/core/execution/` e registre fontes e testes
no `CMakeLists.txt`. Não implemente as operações em `Executor.cpp`, pois esse
arquivo pertence ao dispatcher do pacote 4.

Se uma mudança nos contratos de `Architecture.hpp` ou `Operand.hpp` for
necessária, combine-a antes com os responsáveis pelos pacotes 1, 3 e 4 e
registre a decisão no pull request.

