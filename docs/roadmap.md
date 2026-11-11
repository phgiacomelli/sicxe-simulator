# Roadmap do projeto

## Escopo da primeira etapa — VM concluída

Esta etapa entrega uma VM SIC/XE capaz de executar código de máquina já
codificado e apresentar graficamente o estado da execução.

Não fazem parte desta etapa:

- leitura de assembly;
- tradução de mnemônicos;
- geração de código objeto;
- linking;
- relocação;
- instruções que não pertencem ao conjunto reconhecido pelo simulador.

## Estado da implementação da VM

### Estrutura do projeto

- C++20 e CMake;
- aplicação Qt 6 com bancada visual de execução;
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
[machine-cycle.md](vm/machine-cycle.md) e em
[docs/vm/work/04-machine-control.md](vm/work/04-machine-control.md).

### Entrada de programas e adaptador Qt

- parsers hexadecimal e binário bruto com falhas estruturadas;
- controller com comandos enfileirados e snapshots imutáveis;
- execução contínua e pausa por QTimer;
- testes de entradas, sessão, sinais e responsividade.

Os contratos e decisões estão em [program-input.md](vm/program-input.md).

### Interface gráfica e aceitação

- carga de texto hexadecimal ou arquivo binário bruto com validação pela VM;
- step, execução contínua, pausa e reset com controles condicionados ao estado;
- inspeção de todos os registradores, Condition Code, instrução atual e memória
  por endereço e faixa;
- destaque de mudanças do último passo e diagnóstico tipado persistente;
- testes de widgets e 18 programas de aceitação com bytes codificados.

Os cenários e o fluxo de demonstração estão em [demo.md](vm/demo.md). A
implementação da primeira etapa (VM) está concluída. Não há montador integrado
nem instrução `HALT` sintética.

## Estado dos pacotes da VM

O projeto foi dividido em seis pacotes. O estado de cada um e sua descrição
completa estão no [índice dos pacotes de trabalho](vm/work/README.md).

Os pacotes 1 a 6 estão implementados e cobertos por testes locais. O workflow
da CI verifica formatação e executa um único build Release com Qt e toda a
suíte de testes. A configuração sem Qt permanece disponível para validação
local, mas não é executada na CI. O pacote 6 também passou na CI remota.

## Ordem de integração da VM

1. O pacote 1 estabiliza os tipos arquiteturais e o operando resolvido.
2. Os pacotes 2 e 3 implementam famílias independentes do executor.
3. O pacote 4 integra as famílias ao ciclo da máquina (concluído).
4. O pacote 5 adapta a máquina para a aplicação Qt e para entrada de programas
   (concluído).
5. O pacote 6 conclui a interface e a aceitação ponta a ponta (concluído).

Os pacotes 2, 3, 5 e 6 podem preparar testes e estruturas antes de suas
dependências estarem integradas, mas o merge funcional deve respeitar essa
ordem.

## Segunda etapa — montador de duas passagens implementado localmente

O objetivo é transformar assembly SIC/XE em bytes de máquina e listagem por
duas passagens, com imagem binária bruta fixa para teste na VM. Os pacotes 1–7
estão implementados, incluindo o catálogo como fonte de mnemônicos reservados
para símbolos e a passagem 1 com contador de localização e intermediário
`LocatedLine`, além dos dados e reservas integrados ao layout.
A codificação das instruções está integrada à passagem 2, com `assemble()`,
writers de binário/listagem, CLI e aceitação por execução na VM. O pacote 7
foi validado localmente e revisado tecnicamente, com CI verde no
[PR #30](https://github.com/phgiacomelli/sicxe-simulator/pull/30).
Seu recorte e suas exclusões estão no [índice da segunda etapa](assembler/README.md),
com as interfaces em [contratos](assembler/contracts.md).

Há sete pacotes, um PR por integrante:

1. Fonte, diagnósticos e tipos compartilhados mínimos — integrados; veja
   [fonte do montador](assembler/source.md).
2. Catálogo das 38 instruções, operandos e formato 2 — implementados e
   integrados aos símbolos; veja [ISA do montador](assembler/isa.md).
3. Tabela de símbolos, referências e valores numéricos/simbólicos —
   implementados; veja [símbolos do montador](assembler/symbols.md).
4. Passagem 1 e contador de localização — implementados; veja
   [passagem 1 do montador](assembler/pass-one.md).
5. Dados e reservas — implementados e integrados à passagem 1; veja
   [dados do montador](assembler/data.md).
6. Codificação das instruções XE — implementada e integrada aos módulos
   anteriores; veja
   [codificação](assembler/encoding.md).
7. Passagem 2, binário/listagem, CLI e aceitação — implementados, integrados
   e validados; veja [uso do montador](assembler/usage.md).

O primeiro PR integra contratos e parser; depois 2 e 3 podem ser integrados
independentemente, 4 usa ambos, 5 acrescenta dados ao intermediário, 6
codifica instruções e 7 fecha a saída.
Os sete integrantes podem trabalhar em paralelo nas respectivas branches,
mas os merges funcionais respeitam dependências e CI verde. Nenhuma etapa
introduz assembly na VM, linker ou loader de objeto.

## Terceira etapa — processador de macros em desenvolvimento

O próximo objetivo é processar macros **em uma passagem, antes da montagem**,
sob um módulo principal integrador do macro-montador. A fonte expandida será
publicada como `MASMAPRG.ASM` e entregue ao montador de duas passagens
existente. São exigidas definições dentro de macros e chamadas de macros
dentro de macros. A VM permanece consumindo apenas os bytes finais.

O [índice da etapa](macros/README.md) e os [contratos](macros/contracts.md)
descrevem o método, as convenções de sintaxe e a divisão em sete pacotes.
Ordem de merge: pacote 1; depois 2 e 3 em paralelo; 4; 5; 6; 7. O pacote 1
está **implementado e validado**, com scanner permissivo, posições, tipos de
diagnóstico/origem/resultado e declaração de `expand()`. A revisão técnica e a
CI verde estão registradas no [PR #31](https://github.com/phgiacomelli/sicxe-simulator/pull/31);
o pacote 2 está implementado com captura por nível, esqueleto aninhado e
tabela com versões imutáveis, com validação local, revisão técnica e CI verde
no [PR #32](https://github.com/phgiacomelli/sicxe-simulator/pull/32);
o pacote 3 está implementado com parsing de protótipo/chamada e vinculação
posicional, com validação local, revisão técnica e CI verde no
[PR #33](https://github.com/phgiacomelli/sicxe-simulator/pull/33);
o pacote 4 está implementado e validado localmente, com substituição por
referências de escopo, templates imutáveis e definições internas preparadas;
veja [o handoff de substituição](macros/contracts.md#interfaces-internas-entregues-no-pacote-4).
Os pacotes 5–7 continuam planejados. Ainda não há motor
de expansão, CLI de macros ou geração de `MASMAPRG.ASM`. O PDF de macros é
distribuído à equipe fora do Git, como os PDFs anteriores.
