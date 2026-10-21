# Contratos do processador de macros

Estado: **planejamento, ainda sem implementação**. Este é o ponto único para
decisões compartilhadas. A prioridade é o texto do professor: a expansão
acontece **antes** da montagem, é ativada pelo módulo principal integrador,
aceita definições dentro de macros e chamadas dentro de macros, usa **uma
passagem**, recebe o arquivo-fonte informado para montagem e gera outro
arquivo-fonte chamado exatamente `MASMAPRG.ASM`.

O PDF `ProcessadorDeMacros.pdf` orienta **como** implementar essa exigência.
Como SIC/XE não determina uma sintaxe canônica de macros (ver
[pesquisa](research-sicxe-conventions.md)), as regras de escrita abaixo são
convenções **deste projeto**, escolhidas entre as alternativas dos slides.
Não são novas instruções SIC/XE. Parâmetros por palavra-chave e valores
padrão aparecem nos slides, mas **não fazem parte desta entrega**, porque
o texto prioritário não os exige. A equipe só os acrescentará mediante nova
decisão explícita e atualização deste contrato.

## Seam e direção das dependências

`src/macros/` conterá a biblioteca pura `sicxe_macros`, sem Qt, VM, I/O ou
codificação SIC/XE. Seu seam externo será uma operação equivalente a
`macro::expand(std::string_view sourceText)`, que devolve **ou** texto
expandido e origem das linhas **ou** diagnósticos tipados. O resultado possui
seus dados; chamadas sucessivas não compartilham tabela nem serial.
Detalhes como pilha de parâmetros, tabela de definições e modos são internos
ao módulo e não aparecem na interface do montador.

Um integrador separado em `src/macros/` faz I/O e liga o resultado a
`assembler::assemble(expandedText)`. O fluxo não altera a interface nem os
tipos do montador, não passa texto de macro à VM e não invoca a CLI antiga
como subprocesso. `SICXEAssembler` continua servindo à montagem direta;
o grupo aprovou um novo executável `SICXEMacroAssembler` para a nova entrada.
O texto entregue a `assemble()` é exatamente o publicado em `MASMAPRG.ASM`;
a montagem só começa após a publicação bem-sucedida, sem necessidade de
reabrir o mesmo arquivo.

## Sintaxe mínima adotada pelo projeto

- Diretivas de macro `MCDEFN` e `MCEND`, como nos slides, são reconhecidas
  **somente no campo de operação**, sem distinção de caixa. `MCDEFN` ocupa
  uma linha própria; a próxima linha ativa é o protótipo. `MCEND` fecha o
  nível correspondente. Elas nunca chegam ao montador.
- O protótipo usa os campos de assembly: rótulo formal opcional na coluna 1,
  nome da macro no campo de operação e parâmetros formais no operando. Ex.:

  ```asm
          MCDEFN
  DEST    INCR VAL
  DEST    LDA VAL
          ADD #1
          STA VAL
          MCEND
  MARCA   INCR COUNT
  ```

  Aqui `DEST` recebe `MARCA` e `VAL` recebe `COUNT`. Uma chamada sem rótulo
  deixa o formal de rótulo vazio. Se a macro **não** declara formal de
  rótulo, uma chamada rotulada é erro explícito; não se inventa em qual
  instrução da expansão anexá-lo.
- Nomes de macro e formais usam ASCII `[A-Za-z][A-Za-z0-9_]*`, com comparação
  sem distinção de caixa. Nomes de macro não podem colidir com mnemônicos,
  diretivas do montador ou `MCDEFN`/`MCEND`. A mesma regra de nome reservado
  deve reutilizar a fonte de verdade do montador, sem duplicar a lista das
  38 instruções. Formais repetidos no mesmo protótipo são erro.
- Lista de argumentos é separada por vírgulas **fora de aspas simples**.
  A correspondência é **somente posicional**. Posições vazias e argumentos
  finais omitidos produzem texto vazio; argumentos além dos formais são
  erro. `NOME=valor` não é atribuição de palavra-chave nesta etapa e deve
  receber diagnóstico de sintaxe de macro, tanto no protótipo quanto na
  chamada, não ser interpretado como valor literal. Não acrescentar
  expressão, sintaxe de literal ou modo de endereçamento novo ao montador
  para transportar argumentos.
- A substituição atua nas ocorrências **inteiras** de formais em campos de
  rótulo/operando, nunca em parte de outro identificador, comentário ou
  constante entre aspas. Marcadores SIC/XE existentes, como `#`, `@`, `+`
  e `,X`, conservam sua grafia. Uma expansão pode gerar fonte inválida;
  nesse caso o montador existente dá o diagnóstico de assembly.
- Rótulo local pode usar o sufixo explícito `.SER` **dentro da definição**.
  A expansão substitui `LOOP.SER` por um nome válido como `LOOP_M1` em
  todas as ocorrências da mesma chamada; outra chamada recebe outro serial.
  O serial começa em 1 e cresce por chamada expandida; a forma é
  `NOME_M<serial decimal>` sem preenchimento. Não renomear rótulos comuns
  automaticamente. `.SER` não é aceito no assembly final nem introduz
  sintaxe nova no montador.

## Semântica temporal e aninhamento

O processador percorre uma sequência de linhas de entrada e linhas geradas
por expansões, em ordem observável, sem uma segunda varredura da fonte.
Durante a **definição**, guarda o esqueleto sem emitir linhas. `MCDEFN`
interno incrementa o nível e `MCEND` o decrementa; o nível zero encerra
a captura externa. Parâmetros de definições internas têm seu próprio
escopo: um formal interno com o mesmo nome prevalece no seu corpo; um
formal externo ainda pode substituir ocorrências não sombreadas. Isso
exige referência de parâmetro com profundidade/índice, ou representação
equivalente, antes de expandir a definição externa.

Uma definição interna guardada no corpo externo **não é instalada** nesse
momento. Quando uma chamada à externa a produz, o motor lê essa definição
gerada e a instala. Chamadas no corpo expandido são expandidas por completo
antes de prosseguir. Uma chamada a nome ainda não definido **não é erro do
processador**: ela é copiada sem expansão nem busca posterior, e o montador
rejeitará a operação desconhecida na fonte gerada. Redefinições
substituem a versão ativa somente dali em diante. Essa escolha permite
chamar repetidamente uma macro externa que define a mesma macro interna.
Recursão direta/indireta de chamadas é erro estruturado. Também deve haver
um limite explícito/documentado de profundidade ou volume para impedir
expansão descontrolada, sem truncar silenciosamente a saída.

O motor expande apenas nomes **já ativos** na tabela e copia as demais
operações sem classificá-las. O scanner não rejeita uma linha comum por
sintaxe de assembly: o montador é o único módulo que valida se uma linha
copiada contém mnemônico/diretiva SIC/XE válido, inclusive `+` de formato 4.
A única consulta ao catálogo do montador pelo processador
ocorre ao **definir o nome de uma macro**, para impedir colisão com um
mnemônico/diretiva reservado; não há parsing de operandos SIC/XE no
processador.

## Diagnósticos e origem

Diagnósticos de macro têm código estável, linha/coluna físicas da fonte e
mensagem legível. Exemplos obrigatórios: protótipo ausente/malformado,
`MCEND` sem `MCDEFN`, definição não fechada, nome reservado, argumento
inválido, ciclo e limite de expansão. Não publicar sucesso parcial. O
resultado de sucesso mantém, por linha gerada,
origem na fonte e cadeia de chamadas; o integrador pode assim relacionar
falhas do montador em `MASMAPRG.ASM` à fonte original, preservando também
o número da linha expandida para consulta direta.

Linhas comuns, vazias e comentários são copiadas em ordem. O arquivo
expandido usa LF de modo determinístico; o scanner aceita LF, CRLF e CR.
Comentário inline em uma chamada de macro vira uma linha só de comentário
imediatamente **antes** da expansão; comentários em linhas copiadas ficam
nas próprias linhas. Comentários dentro do corpo são emitidos quando ele
é expandido. Comentários nas linhas estruturais `MCDEFN`, protótipo e
`MCEND` não são emitidos; comentários/linhas vazias entre `MCDEFN` e o
protótipo são ignorados ao buscar a próxima linha ativa. Nenhum comentário
altera a detecção dos campos de macro.

## Arquivos e falhas do integrador

`MASMAPRG.ASM` será publicado no **diretório de trabalho** da invocação, com
esse nome exato. O integrador verifica que o caminho não aponta para a fonte
nem para `.bin`/`.lst`. Um novo conteúdo válido substitui integralmente o
arquivo anterior por publicação temporária e troca atômica, sem truncá-lo
antes de completar a expansão. Falha de leitura, expansão ou publicação
preserva a versão anterior; se não havia arquivo, não deixa um parcial. A
política de arquivo existente da CLI antiga não se aplica a esta saída fixa.
Essa regra deve ser visível em `--help` e testada.

A CLI adotada é `SICXEMacroAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]`,
com `--output`, `--listing`, `--`, `--help`/`-h`, nomes padrão e códigos de
retorno equivalentes aos da [CLI do montador](../assembler/usage.md). Não há
opção de renomear `MASMAPRG.ASM`, pois o nome foi fixado pelo professor.

Após expansão bem-sucedida, a nova versão de `MASMAPRG.ASM` permanece
publicada **mesmo se** a montagem ou a publicação de `.bin/.lst` falhar;
isso permite examinar exatamente a fonte que produziu o erro. Em falha de
montagem, não são publicados `.bin/.lst`. As saídas binária/listagem seguem
a política já definida em
[uso do montador](../assembler/usage.md), com nomes derivados da **fonte
original**, não de `MASMAPRG.ASM`. A CLI antiga permanece compatível.

## Fora do escopo

Não implementar macros de biblioteca, diretivas condicionais, repetição,
expressões de tempo de macro, referências avante, múltiplas passagens,
linker/loader, código objeto relocável, instruções vermelhas, formato 1,
modos SIC/XE novos ou integração da VM à sintaxe de assembly. Os exemplos
dos slides são material do método, não autorização para outros opcodes.
