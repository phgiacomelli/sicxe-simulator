# Pacote 5 — Entrada de programas e adaptação Qt

Este documento registra a entrega do pacote 5. As referências à interface como
trabalho posterior descrevem o estado na época; a interface foi implementada
no [pacote 6](06-interface-acceptance.md).

## Resultado esperado

Criar o adaptador entre a `Machine` C++ e a aplicação Qt, incluindo as formas de
entrada usadas na demonstração. A interface gráfica deve comandar a máquina sem
receber referências mutáveis ao estado interno.

## Dependências

- Interface pública estável da `Machine`, entregue pelo pacote 4.
- Formato de snapshots e falhas definido no núcleo.

Parsers de entrada podem ser desenvolvidos antes da máquina estar pronta.

## Ownership

Arquivos sugeridos:

```text
src/application/ProgramInput.hpp
src/application/HexParser.cpp
src/application/HexParser.hpp
src/application/RawBinaryReader.cpp
src/application/RawBinaryReader.hpp
src/application/MachineController.cpp
src/application/MachineController.hpp
tests/application/HexParserTest.cpp
tests/application/RawBinaryReaderTest.cpp
tests/application/MachineControllerTest.cpp
docs/vm/program-input.md
```

Este pacote pode alterar o CMake para adicionar o módulo de aplicação e QTest,
mas não deve mover regras da VM para classes Qt.

## Interface de entrada

As duas origens devem produzir o mesmo valor de programa:

- texto hexadecimal com bytes separados por espaços e quebras de linha;
- arquivo binário bruto, preservando todos os bytes.

O valor entregue à máquina deve conter bytes, endereço de carga e PC inicial.
Erros de parsing devem indicar a causa sem alterar a máquina atual.

## Adaptador Qt

O `MachineController` deve:

- receber comandos da UI para carregar, resetar e executar passos;
- devolver snapshots imutáveis;
- converter resultados em sinais sem transformar falhas em texto cedo demais;
- manter a UI responsiva durante execução contínua;
- serializar comandos para evitar dois passos simultâneos;
- permitir pausa somente entre instruções.

A estratégia de responsividade pode usar `QTimer` ou worker thread. A escolha
deve ser registrada no PR com o motivo e testada; não devem existir duas
estratégias simultâneas.

## Sequência sugerida

1. Implementar o valor comum de entrada de programa.
2. Implementar e testar o parser hexadecimal.
3. Implementar e testar a leitura binária.
4. Definir os comandos e sinais do controller.
5. Implementar step, reset e carga.
6. Implementar execução contínua e pausa.
7. Documentar formatos e comportamento de erro.

## Testes obrigatórios

- texto vazio, espaços, quebras de linha e mistura de maiúsculas/minúsculas;
- dígito inválido, byte incompleto e valor acima de `FF`;
- arquivo vazio e arquivo contendo todos os valores `00..FF`;
- endereço de carga e PC inválidos;
- entrada inválida preserva a sessão atual;
- cada comando chega uma única vez à máquina;
- snapshots e detalhes de falha são preservados;
- execução contínua mantém o loop de eventos responsivo;
- pausa ocorre em fronteira de instrução;
- núcleo continua compilando sem dependência de Qt.

## Fora do escopo

- widgets e layout visual;
- interpretação de mnemônicos;
- formato objeto, símbolos e relocação;
- alteração direta de memória ou registradores pela UI;
- semântica das instruções.

## Critério de conclusão

- [x] Texto hexadecimal e binário produzem a mesma representação de programa.
- [x] Erros de entrada são estruturados e testados.
- [x] Controller não expõe estado mutável da máquina.
- [x] Execução contínua não congela a UI.
- [x] Núcleo não inclui headers nem bibliotecas Qt.
- [x] `docs/vm/program-input.md` descreve formatos e exemplos.
- [x] CI verde e revisão externa (interface pronta para o pacote 6).

## Entrega e decisões

Implementado com `QTimer`, comandos enfileirados e snapshots imutáveis.
Contratos, alternativas e limitações estão centralizados em
[program-input.md](../program-input.md). Nenhuma interface do núcleo foi alterada
e nenhuma divergência adicional com o enunciado foi encontrada. CI e revisão
externa foram concluídas; widgets e aceitação visual permanecem no pacote 6.
