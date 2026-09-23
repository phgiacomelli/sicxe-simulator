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
- reset dos valores;
- acesso tipado aos registradores inteiros;
- representação e atualização do Condition Code;
- testes unitários.

### Decoder

- 38 instruções reconhecidas;
- formatos SIC, 2, 3 e 4;
- flags e campos de operandos;
- validação de registradores e codificações;
- falhas estruturadas;
- testes unitários.

### Resolução de operandos

- operandos tipados para registradores, imediato, memória e ausência de
  operando;
- endereçamento SIC, direto, PC-relative, base-relative e formato 4;
- indexação, imediato e indireto;
- validação de limites sem wrap;
- falhas estruturadas;
- testes unitários.

### Transferência de dados e lógica

- `CLEAR` e `RMO`;
- loads e stores de palavra;
- `LDCH` e `STCH`;
- `AND` e `OR`;
- efeitos sem mutação direta da máquina;
- falhas estruturadas e testes unitários.

## Próximo incremento

Concluir as operações aritméticas e relacionais do pacote 3. Em seguida, o
pacote 4 integrará as famílias de operações ao dispatcher e ao ciclo atômico da
máquina.

## Trabalho restante

O trabalho restante foi dividido em seis pacotes. A descrição completa de cada
um está no [índice dos pacotes de trabalho](work/README.md).

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
