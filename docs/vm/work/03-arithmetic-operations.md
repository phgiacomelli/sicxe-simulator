# Pacote 3 — Aritmética, comparações, shifts e indexação

## Resultado esperado

Implementar as 14 operações aritméticas e relacionais com semântica explícita
de inteiros de 24 bits, incluindo Condition Code e falha de divisão por zero.

## Instruções sob ownership

```text
ADD ADDR SUB SUBR MUL MULR DIV DIVR
COMP COMPR
SHIFTL SHIFTR
TIX TIXR
```

## Dependências

- `ResolvedOperand`, helpers de registradores e CC do pacote 1.
- Contrato de `ExecutionEffect` aprovado com os pacotes 2 e 4.

O efeito, as falhas e a regra do PC entregues pelo pacote 1 estão em
[contratos de execução](../execution-contracts.md).

## Ownership

Arquivos sugeridos:

```text
src/core/execution/ArithmeticOperations.cpp
src/core/execution/ArithmeticOperations.hpp
tests/ArithmeticOperationsTest.cpp
docs/vm/execution-arithmetic.md
```

## Interface a fornecer

Uma função pura equivalente a:

```cpp
ExecuteResult executeArithmeticOperation(const DecodedInstruction& instruction,
                                         const ResolvedOperand& operand,
                                         std::uint32_t postFetchPc,
                                         const Memory& memory,
                                         const RegisterFile& registers);
```

## Semântica de 24 bits

- Aritmética e comparação interpretam padrões em complemento de dois no
  intervalo `-8388608..8388607`.
- Resultados são normalizados para os 24 bits inferiores.
- Overflow comum produz wrap de 24 bits.
- Divisão trunca em direção a zero.
- Divisor zero retorna falha e nenhum efeito parcial.
- Conversões signed e operações de shift devem evitar comportamento indefinido
  do C++.

## Comportamento obrigatório

- Operações de registrador seguem `r2 ← r2 operação r1`.
- `COMPR` compara `r1` com `r2` e altera somente CC.
- `COMP` compara `A` com o operando e altera somente CC.
- `SHIFTL` faz rotação circular de 24 bits para a esquerda.
- `SHIFTR` faz deslocamento aritmético para a direita.
- A contagem efetiva de shift é o nibble decodificado mais um, de 1 a 16.
- `TIX` e `TIXR` incrementam `X` e atualizam CC no mesmo efeito.
- O próximo PC padrão é o PC pós-fetch.

## Sequência sugerida

1. Implementar e testar conversões signed/unsigned de 24 bits.
2. Implementar comparação e atualização de CC.
3. Implementar soma e subtração.
4. Implementar multiplicação e divisão.
5. Implementar shifts.
6. Implementar `TIX` e `TIXR`.
7. Documentar os casos-limite.

## Testes obrigatórios

- pelo menos um teste independente por opcode;
- zero, um, menos um, mínimo e máximo signed de 24 bits;
- overflow positivo e negativo;
- `-8388608 / -1` normalizado para 24 bits;
- divisão positiva e negativa com truncamento;
- divisor zero em memória e registrador;
- três resultados de CC e preservação dos demais bits de `SW`;
- direção de `r1` e `r2` em todas as operações de registrador;
- shifts com contagens 1 e 16;
- rotação com bits saindo da palavra;
- deslocamento aritmético de valor negativo;
- efeito duplo de `TIX` e `TIXR` sem mutação antecipada.

## Fora do escopo

- ponto flutuante e registrador `F` como operando;
- cálculo de endereço;
- saltos;
- commit dos efeitos;
- interface Qt.

## Critério de conclusão

- [x] As 14 instruções possuem implementação e testes de limite.
- [x] Toda aritmética é definida para padrões de 24 bits.
- [x] Divisão por zero não produz efeito arquitetural.
- [x] CC é atualizado somente pelo helper compartilhado.
- [x] Nenhuma operação altera diretamente o estado.
- [x] `docs/vm/execution-arithmetic.md` registra a semântica e os limites.
- [x] CI verde e integração revisada pelo responsável do pacote 4.

## Decisões e estado final

- Operações signed convertem padrões de 24 bits para tipos intermediários mais
  largos antes de calcular e normalizam o resultado com `WordMask`.
- Divisão trunca em direção a zero; divisor zero imediato, em memória ou em
  registrador produz `DivisionByZero` sem efeito parcial.
- Nibbles de shift fora do intervalo codificado `0..15` são rejeitados como
  `InvalidOperand`.
- O dispatcher, o commit dos efeitos e as operações de controle de fluxo foram
  integrados pelo pacote 4, conforme documentado no
  [ciclo da máquina](../machine-cycle.md).
