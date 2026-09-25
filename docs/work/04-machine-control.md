# Pacote 4 — Controle de fluxo e ciclo da `Machine`

## Resultado esperado

Integrar decoder, resolver e famílias de execução em um ciclo atômico, além de
implementar as seis operações de controle de fluxo. A `Machine` será a única
dona das escritas no estado arquitetural.

## Instruções sob ownership

```text
J JEQ JGT JLT JSUB RSUB
```

## Dependências

- Tipos e `AddressResolver` do pacote 1.
- `ExecutionEffect`, `ExecuteFault` e regra do PC pós-fetch entregues pelo
  pacote 1.
- Operações dos pacotes 2 e 3.
- `Decoder`, `Memory` e `RegisterFile` existentes.

O esqueleto e os testes de estado podem começar antes das dependências, mas o
merge funcional ocorre depois dos pacotes 1, 2 e 3.

## Ponto de partida atual

Os três pacotes necessários estão concluídos. O pacote 4 deve consumir, sem
reimplementar suas regras:

- `Decoder::decode` para obter `DecodedInstruction`;
- `computePostFetchPc` para calcular o PC sequencial sem wrap;
- `AddressResolver::resolve` para produzir `ResolvedOperand`;
- `executeDataOperation` para as 18 operações de transferência e lógica;
- `executeArithmeticOperation` para as 14 operações aritméticas;
- `ExecutionEffect` e `ExecuteFault` como resultado comum das três famílias.

`OperationSupport.hpp` é um detalhe interno das famílias de operações, não uma
interface do dispatcher. O pacote 4 deve depender das interfaces públicas de
cada família.

## Ownership

Arquivos atuais:

```text
src/core/Executor.cpp
src/core/Executor.hpp
src/core/Machine.cpp
src/core/Machine.hpp
```

Arquivos sugeridos:

```text
src/core/MachineFault.hpp
src/core/execution/ControlOperations.cpp
src/core/execution/ControlOperations.hpp
tests/ControlOperationsTest.cpp
tests/ExecutorTest.cpp
tests/MachineTest.cpp
docs/machine-cycle.md
```

## Interfaces a fornecer

`Executor` deve despachar para as três famílias e retornar `ExecutionEffect` ou
falha. `Machine` deve expor, no mínimo, operações equivalentes a:

```cpp
LoadResult loadProgram(std::span<const std::uint8_t> bytes,
                       std::uint32_t loadAddress,
                       std::uint32_t initialPc);
void reset();
StepResult step();
MachineSnapshot snapshot() const;
```

Execução contínua pode ser fornecida pela máquina ou pelo adaptador Qt, mas a
decisão deve ser registrada antes do pacote 5 iniciar a integração.

## Ciclo obrigatório

1. Capturar o PC da instrução.
2. Decodificar sem alterar estado.
3. Calcular o PC pós-fetch em tipo largo.
4. Resolver o operando.
5. Executar e produzir um efeito.
6. Validar todos os destinos do efeito.
7. Confirmar registradores, memória e PC.

Qualquer falha entre os passos 2 e 6 mantém memória, registradores e PC como
estavam antes da tentativa.

Antes de escrever o commit, feche a decisão registrada em
[contratos de execução](../execution-contracts.md#validação-e-commit) sobre
operações de formato 2 que usam `PC` ou `SW`. A solução deve possuir testes e
não pode depender da ordem acidental entre `registerWrite`, `conditionCode` e
`nextPc`.

## Controle de fluxo

- `J` sempre grava o destino resolvido no PC.
- `JEQ`, `JGT` e `JLT` consultam o CC.
- `JSUB` grava o PC pós-fetch em `L` e transfere para o destino.
- `RSUB` transfere para o endereço armazenado em `L`.
- Quando não há salto, o próximo PC é o PC pós-fetch.

## Carga e reset

- Carga válida zera o estado, copia bytes contíguos e define o PC inicial.
- Entrada vazia ou intervalo inválido é rejeitado antes de alterar a máquina.
- `reset` zera memória e registradores e remove falhas anteriores.
- A memória deve continuar com exatamente 1 MiB na composição padrão.

## Testes obrigatórios

- carga válida nos limites e rejeições sem alteração parcial;
- reset completo;
- avanço de PC em formatos 2, 3 e 4;
- um passo bem-sucedido de cada família do executor;
- cada salto tomado e não tomado;
- `JSUB` e `RSUB` com endereço de retorno correto;
- opcode desconhecido, instrução truncada e endereço inválido;
- divisão por zero sem efeitos parciais;
- escrita de memória no limite;
- snapshot antes/depois de sucesso e falha;
- validação de efeitos malformados antes do commit.

## Fora do escopo

- parsing de texto hexadecimal ou arquivo;
- tipos Qt, threads e widgets;
- regras internas das operações dos pacotes 2 e 3;
- montagem ou relocação.

## Critério de conclusão

- [x] As seis instruções de controle possuem testes próprios.
- [x] O ciclo completo executa as 38 instruções por meio do dispatcher.
- [x] Toda falha observável preserva o estado arquitetural.
- [x] Carga e reset possuem comportamento documentado e testado.
- [x] A interface pública necessária ao pacote 5 está estável.
- [x] `docs/machine-cycle.md` explica pipeline, commit e falhas.
- [x] CI verde e testes de integração passam em build limpa.

### Verificação local

A build limpa e a suíte completa foram executadas localmente com CMake/CTest.
Resultado: 212/212 testes passaram.

A CI remota e a revisão externa foram concluídas antes do merge do pacote 4.
