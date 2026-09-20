# Roadmap da primeira etapa

## Escopo

Esta etapa entrega uma VM SIC/XE capaz de executar código de máquina já
codificado e apresentar graficamente o estado da execução.

Não fazem parte desta etapa:

- leitura de assembly;
- tradução de mnemônicos;
- geração de código objeto;
- linking;
- relocação;
- instruções que não pertencem ao conjunto reconhecido pelo simulador.

## Concluído

### Estrutura do projeto

- C++20 e CMake;
- aplicação Qt 6 inicial;
- GoogleTest integrado ao CTest;
- formatação com `clang-format`.

### Memória

- acesso por byte;
- palavras de 24 bits;
- ordem big-endian;
- validação de limites;
- testes unitários.

### Registradores

- registradores arquiteturais;
- normalização de escritas para 24 ou 48 bits;
- reset dos valores.

### Decoder

- 38 instruções reconhecidas;
- formatos SIC, 2, 3 e 4;
- flags e campos de operandos;
- validação de registradores e codificações;
- falhas estruturadas;
- testes unitários.

## Próximo incremento

Implementar o `AddressResolver`.

O incremento deve introduzir um resultado tipado capaz de representar:

- ausência de operando;
- operandos de registradores;
- valor imediato;
- endereço de memória;
- falha de endereço.

Em seguida, implementar e testar:

1. formato 2 e instrução sem operando;
2. SIC direto e indexado;
3. formato 3 direto;
4. relativo ao PC, incluindo deslocamentos negativos;
5. relativo à base;
6. formato 4;
7. indexação por `X`;
8. imediato;
9. indireto;
10. limites da memória.

## Trabalho restante

O trabalho restante foi dividido em seis pacotes. A descrição completa de cada
um está no [índice dos pacotes de trabalho](work/README.md).

### Operações de transferência e lógica

`CLEAR`, `RMO`, loads, stores, `LDCH`, `STCH`, `AND` e `OR`.

### Operações aritméticas e relacionais

`ADD`, `ADDR`, `SUB`, `SUBR`, `MUL`, `MULR`, `DIV`, `DIVR`, `COMP`, `COMPR`,
`SHIFTL`, `SHIFTR`, `TIX` e `TIXR`.

### Controle de fluxo

`J`, `JEQ`, `JGT`, `JLT`, `JSUB` e `RSUB`.

### Ciclo da máquina

- carga do programa;
- reset completo;
- fetch, decode, resolve e execute;
- atualização do PC;
- execução passo a passo;
- execução contínua e pausa;
- tratamento de falhas sem alterações parciais.

### Interface gráfica

- entrada de bytes codificados;
- endereço inicial;
- controles de execução;
- registradores e Condition Code;
- visualização da memória;
- instrução corrente;
- apresentação de erros.

### Verificação final

- testes do arquivo de registradores;
- um teste independente por instrução;
- testes de integração da máquina;
- pequenos programas completos já codificados;
- testes da interface;
- build reproduzível a partir de um checkout limpo.

## Ordem de integração

1. O pacote 1 estabiliza os tipos arquiteturais e o operando resolvido.
2. Os pacotes 2 e 3 implementam famílias independentes do executor.
3. O pacote 4 integra as famílias ao ciclo da máquina.
4. O pacote 5 adapta a máquina para a aplicação Qt e para entrada de programas.
5. O pacote 6 conclui a interface e a aceitação ponta a ponta.

Os pacotes 2, 3, 5 e 6 podem preparar testes e estruturas antes de suas
dependências estarem integradas, mas o merge funcional deve respeitar essa
ordem.
