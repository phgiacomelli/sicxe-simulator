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

## Estado da implementação

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

### Aritmética, comparações, shifts e indexação

- operações aritméticas de memória e registrador;
- comparação signed de 24 bits;
- divisão por zero como falha estruturada;
- rotação e deslocamento aritmético;
- incremento de `X` e atualização atômica do Condition Code;
- testes de limites e ausência de mutação antecipada.

### Controle de fluxo e ciclo da máquina

- `J`, `JEQ`, `JGT`, `JLT`, `JSUB` e `RSUB`;
- dispatcher das três famílias de operações;
- ciclo atômico com validação do efeito antes do commit;
- carga de programa, reset, snapshot e passo a passo;
- falhas estruturadas sem alterações parciais;
- testes unitários e de integração.

A execução de cada critério do pacote está em
[machine-cycle.md](machine-cycle.md) e em
[docs/work/04-machine-control.md](work/04-machine-control.md).

### Entrada de programas e adaptador Qt

- parsers hexadecimal e binário bruto com falhas estruturadas;
- controller com comandos enfileirados e snapshots imutáveis;
- execução contínua e pausa por QTimer;
- testes de entradas, sessão, sinais e responsividade.

Os contratos e decisões estão em [program-input.md](program-input.md).

## Próximo incremento

Implementar a interface gráfica e aceitação ponta a ponta, conforme o pacote 6.

## Trabalho restante

O projeto foi dividido em seis pacotes. O estado de cada um e sua descrição
completa estão no [índice dos pacotes de trabalho](work/README.md).

### Execução contínua

Implementadas pelo adaptador do pacote 5; a `Machine` mantém apenas o passo
atômico. Ver [contrato do controller](program-input.md).

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
3. O pacote 4 integra as famílias ao ciclo da máquina (concluído).
4. O pacote 5 adapta a máquina para a aplicação Qt e para entrada de programas
   (concluído).
5. O pacote 6 conclui a interface e a aceitação ponta a ponta.

Os pacotes 2, 3, 5 e 6 podem preparar testes e estruturas antes de suas
dependências estarem integradas, mas o merge funcional deve respeitar essa
ordem.
