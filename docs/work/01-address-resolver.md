# Pacote 1 — Base arquitetural e `AddressResolver`

## Resultado esperado

Entregar a representação compartilhada de registradores, Condition Code e
operandos resolvidos, além de um `AddressResolver` completo e testado. Ao final,
os módulos de execução devem consumir operandos sem repetir cálculos de
endereçamento.

## Dependências

- `Memory`, `RegisterFile` e `Decoder` existentes.
- Decisões registradas em `docs/decoder.md`.

Este pacote deve ser integrado antes da implementação final dos pacotes 2 e 3.

## Ownership

Arquivos atuais:

```text
src/core/AddressResolver.cpp
src/core/AddressResolver.hpp
src/core/RegisterFile.cpp
src/core/RegisterFile.hpp
```

Arquivos sugeridos:

```text
src/core/Architecture.hpp
src/core/Operand.hpp
tests/RegisterFileTest.cpp
tests/AddressResolverTest.cpp
docs/addressing.md
```

Alterações em `Instruction.hpp` devem ser mínimas e justificadas, pois o decoder
já está concluído.

## Interface a fornecer

O resolver deve expor uma única operação equivalente a:

```cpp
ResolveResult resolve(const DecodedInstruction& instruction,
                      std::uint32_t postFetchPc,
                      const Memory& memory,
                      const RegisterFile& registers) const;
```

O nome exato dos tipos pode mudar no PR de contratos, mas o resultado deve
distinguir:

- instrução sem operando;
- operandos de registradores;
- valor imediato;
- localização de memória;
- falha de resolução.

O resolver retorna valores e não altera memória, registradores ou PC.

## Comportamento obrigatório

### Registradores e CC

- Permitir leitura e escrita tipada de `A`, `X`, `L`, `B`, `S`, `T`, `PC` e
  `SW` pelos códigos aceitos pelo decoder.
- Manter `F` armazenado e resetável, mas fora dos operandos das instruções do
  recorte.
- Centralizar a conversão entre `Less`, `Equal`, `Greater` e os bits escolhidos
  de `SW`.
- Preservar os demais bits de `SW` ao atualizar CC.

### Cálculo do endereço

- SIC: endereço unsigned de 15 bits, seguido de indexação quando `x=1`.
- Formato 3 direto: campo unsigned de 12 bits.
- PC-relative: extensão de sinal de 12 bits somada ao PC pós-fetch.
- Base-relative: deslocamento unsigned somado a `B`.
- Formato 4: endereço unsigned de 20 bits.
- Indexado: somar `X` depois do cálculo principal.
- Imediato: devolver o valor calculado sem leitura final de memória.
- Simples: devolver a localização calculada.
- Indireto: ler uma palavra na localização calculada e usá-la como endereço.

Toda soma deve ser feita em tipo largo o suficiente para detectar overflow. Um
resultado fora da memória é falha; não deve ocorrer wrap silencioso.

## Sequência sugerida

1. Criar testes do `RegisterFile` e do CC.
2. Propor `ResolvedOperand`, `ResolveResult` e a falha de endereço.
3. Implementar formato 2 e ausência de operando.
4. Implementar SIC, direto, PC-relative e base-relative.
5. Implementar formato 4 e indexação.
6. Implementar imediato e indireto.
7. Documentar exemplos e limites.

## Testes obrigatórios

- reset e máscaras do `RegisterFile`, incluindo `F` com 48 bits;
- leitura/escrita pelos códigos aceitos;
- três valores de CC e preservação dos outros bits de `SW`;
- cada cálculo de endereço listado acima;
- deslocamento PC-relative positivo, zero e negativo;
- soma por `B` e `X` no limite inferior e superior;
- palavra usada como ponteiro indireto;
- ponteiro indireto fora da memória;
- palavra indireta que cruza o fim da memória;
- ausência de mutação em sucesso e falha.

## Fora do escopo

- ler o valor final usado por uma operação simples;
- executar opcodes;
- avançar ou gravar o PC;
- carregar programas;
- tipos ou sinais Qt.

## Critério de conclusão

- [ ] Interface aprovada pelos responsáveis dos pacotes 2, 3 e 4.
- [ ] Todos os modos aceitos pelo decoder possuem teste positivo.
- [ ] Limites e falhas de endereço possuem testes negativos.
- [ ] `RegisterFile` e CC possuem testes próprios.
- [ ] Nenhum método do resolver modifica estado arquitetural.
- [ ] `docs/addressing.md` descreve o cálculo sem copiar a implementação.
- [ ] CI verde e pendências registradas no PR.
