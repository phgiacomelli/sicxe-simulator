# Pacote 2 — Transferência de dados e operações lógicas

## Resultado esperado

Implementar a semântica das 18 operações de transferência e lógica como um
módulo testável, sem calcular endereços e sem modificar diretamente o estado da
máquina.

## Instruções sob ownership

```text
CLEAR RMO
LDA LDB LDL LDS LDT LDX
STA STB STL STS STT STX
LDCH STCH
AND OR
```

## Dependências

- `DecodedInstruction` entregue pelo decoder.
- `ResolvedOperand` e acesso tipado aos registradores entregues pelo pacote 1.
- Contrato de `ExecutionEffect` aprovado com os pacotes 3 e 4.

O desenvolvimento pode começar com testes e funções puras após o merge dos
contratos. A integração ao dispatcher pertence ao pacote 4.

Os contratos concretos e cuidados de integração entregues pelo pacote 1 estão
no [guia de integração do pacote 2](02-data-operations-integration.md).
O efeito, as falhas e a regra do PC estão em
[contratos de execução](../execution-contracts.md).

## Ownership

Arquivos sugeridos:

```text
src/core/execution/DataOperations.cpp
src/core/execution/DataOperations.hpp
tests/DataOperationsTest.cpp
docs/execution-data.md
```

O responsável não deve implementar estas operações diretamente em
`Executor.cpp`, evitando conflito com os pacotes 3 e 4.

## Interface a fornecer

Uma operação interna equivalente a:

```cpp
ExecuteResult executeDataOperation(const DecodedInstruction& instruction,
                                   const ResolvedOperand& operand,
                                   std::uint32_t postFetchPc,
                                   const Memory& memory,
                                   const RegisterFile& registers);
```

O resultado deve descrever o próximo PC e as alterações pretendidas. A função
não confirma escritas em memória ou registradores.

## Comportamento obrigatório

- Loads de palavra copiam exatamente 24 bits da memória para o registrador.
- Stores de palavra produzem uma escrita de três bytes em big-endian.
- `LDCH` substitui apenas os 8 bits menos significativos de `A`.
- `STCH` grava apenas os 8 bits menos significativos de `A`.
- `CLEAR` zera o registrador de 24 bits indicado por `r1`.
- `RMO` produz `r2 ← r1`.
- `AND` e `OR` operam sobre padrões de 24 bits.
- O próximo PC padrão é o PC pós-fetch recebido.
- Nenhuma dessas instruções altera CC.
- Um operando imediato fornece o valor diretamente para loads e operações
  lógicas.
- Stores usam a localização resolvida; combinações incompatíveis devem retornar
  falha de execução, não lançar exceção de memória para o chamador.

## Sequência sugerida

1. Criar helpers de leitura de valor e produção de escrita usando
   `ResolvedOperand`.
2. Implementar `CLEAR` e `RMO`.
3. Implementar loads e stores de palavra.
4. Implementar `LDCH` e `STCH`.
5. Implementar `AND` e `OR`.
6. Completar a matriz de testes e documentação.

## Testes obrigatórios

- pelo menos um teste independente para cada opcode;
- imediato, simples e indireto nas operações em que se aplicam;
- direção correta de `RMO`;
- carregamento dos padrões `0x000000`, `0xFFFFFF` e `0x800000`;
- preservação dos 16 bits superiores em `LDCH`;
- escrita de apenas um byte em `STCH`;
- endereço inicial e endereço final válido da memória;
- tipo de operando incompatível;
- efeito contém somente os registradores e bytes esperados;
- nenhuma mutação antes do commit da `Machine`.

## Fora do escopo

- cálculo de endereço;
- aritmética, comparação e shifts;
- saltos e atualização condicional do PC;
- commit do efeito;
- interface Qt.

## Critério de conclusão

- [ ] As 18 instruções possuem implementação e teste próprio.
- [ ] Não existe lógica de `n/i/x/b/p/e` neste módulo.
- [ ] Nenhuma operação modifica diretamente `Memory` ou `RegisterFile`.
- [ ] Efeitos de byte e palavra são diferenciados.
- [ ] `docs/execution-data.md` contém a matriz fonte/destino/largura.
- [ ] CI verde e integração revisada pelo responsável do pacote 4.
