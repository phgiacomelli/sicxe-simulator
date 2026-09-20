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

- [ ] As seis instruções de controle possuem testes próprios.
- [ ] O ciclo completo executa as 38 instruções por meio do dispatcher.
- [ ] Toda falha observável preserva o estado arquitetural.
- [ ] Carga e reset possuem comportamento documentado e testado.
- [ ] A interface pública necessária ao pacote 5 está estável.
- [ ] `docs/machine-cycle.md` explica pipeline, commit e falhas.
- [ ] CI verde e testes de integração passam em build limpa.
