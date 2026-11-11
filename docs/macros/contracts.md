# Contratos do processador de macros

Estado: **pacotes 1–4 implementados e validados; pacotes 5–7 planejados**.
Scanner, captura, tabela, vinculação e substituição com escopo disponíveis;
`expand()` está somente declarada, sem
motor ou CLI de macros. Revisão técnica e CI verde do pacote 1 registradas no
[PR #31](https://github.com/phgiacomelli/sicxe-simulator/pull/31), e as do pacote 2
no [PR #32](https://github.com/phgiacomelli/sicxe-simulator/pull/32). O pacote 3
tem revisão técnica e CI verde no [PR #33](https://github.com/phgiacomelli/sicxe-simulator/pull/33).
O pacote 4 tem revisão, testes locais e integração registrados no
[PR #34](https://github.com/phgiacomelli/sicxe-simulator/pull/34).
Este é o ponto
único para decisões compartilhadas. A prioridade é o texto do professor: a expansão
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

`src/macros/` contém a biblioteca pura `sicxe_macros`, sem Qt, VM, I/O ou
codificação SIC/XE. O pacote 1 também não depende do montador. Seu seam externo
está declarado como `macro::expand(std::string_view sourceText)`, que devolverá **ou** texto
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

## Interfaces C++ entregues no pacote 1

Os tipos abaixo vivem no namespace `macro`, separados dos tipos do montador.
Headers usam `#pragma once`; todos os textos e vetores pertencem ao resultado.

### Scanner e posições

`SourceScanner::scan(std::string_view)` retorna `SourceScanResult`, com
`lines` em ordem física. Não consulta o catálogo nem chama `SourceParser`.
O scanner percorre os bytes da fonte sem coletar definições ou classificar
chamadas: a leitura lexical não antecipa a visibilidade das macros do motor.

- `SourcePosition { line, column }`: ambos iniciados em 1, coluna de byte,
  inclusive para tabs e texto UTF-8. Não é posição visual na interface.
- `SourceField { text, position }`: grafia original do campo e seu início.
- `SourceLine`: `number`, `text` sem terminador, `kind`, campos opcionais
  `label`, `operation`, `operand`, `comment`, além de `openQuote` opcional e
  `terminated`. `kind` é `Blank`, `Comment` ou `Code`; **Code não significa
  assembly válido**. Campos ausentes continuam opcionais; não há inferência
  de operação a partir de um token sem recuo na primeira coluna.
- O rótulo é o primeiro token quando começa na coluna 1. Operação é o token
  seguinte, ou o primeiro após recuo. O restante antes do comentário é um
  único operando opaco, sem os espaços/tabs externos; espaços internos e
  vírgulas são preservados. O texto integral mantém inclusive espaços finais.
- `comment` inclui o marcador `.` ou `;`. Uma linha inteira de comentário
  não expõe campos de código. Aspas em comentários não abrem constantes.
- `openQuote` guarda a posição da aspa simples que ficou aberta na linha.
  É informação lexical, não uma falha automática. O estado não atravessa a
  quebra de linha. O consumidor decide se houve erro de sintaxe de macro;
  assembly comum inválido continua sendo responsabilidade do montador.
- `terminated` indica se havia LF, CRLF ou CR após a linha. Fonte vazia tem
  zero linhas; terminador final não produz uma linha fictícia adicional.
- `SourceLine::directive()` retorna `MacroDirective::DefinitionBegin` para
  operação `MCDEFN`, `DefinitionEnd` para `MCEND`, ou `None`. A comparação
  ASCII ignora caixa, mas exige token completo: `+MCDEFN` não é diretiva.
  Reconhecer não valida rótulo/operando indevido ou pareamento de níveis.
- `SourceScanResult::normalizedText()` copia `text` e substitui cada
  terminador existente por LF, preservando ausência de terminador final.
  **Não é expansão**: mantém diretivas, parâmetros, `.SER` e comentários,
  inclusive comentários estruturais que serão tratados por outros pacotes.

O scanner aceita campos temporariamente inválidos, inclusive rótulos com
`.SER`, operações desconhecidas, parâmetros e operandos incompletos. Não
divide argumentos nem interpreta `NOME=valor`: o pacote 3 rejeita essa sintaxe
em protótipos/chamadas. Não existe `succeeded()`
no resultado lexical, pois o scanner não certifica validade de macro/assembly.

### Origem, diagnósticos e resultado futuro

`SourceOrigin.hpp` define:

- `ExpansionFrame { macroName, definition, invocation }`: nome da macro,
  posição da operação do protótipo e posição da operação da chamada, ambas
  na fonte original. Para chamada aninhada, `invocation` pode estar no corpo
  de uma definição; a cadeia mantém as chamadas que levaram a esse ponto.
- `SourceOrigin { expandedLine, source, calls }`: número físico de saída
  começando em 1, posição inicial da linha original que forneceu o texto e
  cadeia de chamadas da mais externa à mais interna. Linha copiada diretamente
  tem cadeia vazia. Linhas de comentário destacadas podem apontar ao marcador
  original. Isso é rastreabilidade por linha, não um mapa exato de colunas
  após substituição de textos de comprimentos diferentes.

`MacroDiagnostic { code, position, message, calls }` possui código tipado,
posição na fonte original, mensagem e cadeia de chamadas na mesma ordem.
Os códigos iniciais são `MissingPrototype`, `InvalidPrototype`, `UnexpectedEnd`,
`UnterminatedDefinition`, `ReservedName`, `InvalidArgument`,
`RecursiveExpansion` e `ExpansionLimitExceeded`. Representam categorias já
previstas neste contrato; **o scanner não as emite**. Os pacotes responsáveis
implementarão a detecção e poderão acrescentar códigos específicos, documentados
e revisados pelos consumidores. Testes usam código/posição, não mensagem literal
como identificador de erro; os valores numéricos do enum não são formato externo.

`MacroResult.hpp` define:

- `MacroSuccess { expandedText, origins }`: texto LF e uma origem por linha
  física emitida, em ordem, com `expandedLine` consecutivo. Fonte expandida
  vazia pode ter texto e mapa vazios.
- `MacroFailure { diagnostics }`: somente diagnósticos, sem texto parcial.
- `MacroResult = std::variant<MacroSuccess, MacroFailure>`.

Esses tipos são valores; não validam por si sós as invariantes do produtor.
O contexto dos pacotes 5/6 produzirá o mapa; o integrador do pacote 7 fará
o remapeamento dos erros do montador sem alterar seus tipos existentes.

`MacroProcessor.hpp` declara `MacroResult expand(std::string_view sourceText)`.
**Ainda não há definição linkável dessa função**: ela pertence ao pacote 6.
Consumidores atuais podem usar scanner e tipos, não chamar a expansão. Não há
stub de sucesso, código de erro provisório ou implementação que devolva uma
fonte ainda com macros como se tivesse sido expandida.

Na integração, alinhar os campos de linha com os pacotes 2/3, os tipos de
origem com o pacote 5 e o resultado público com o pacote 6. Pilha interna,
tabela de definições e estratégias de substituição não fazem parte desse seam.

## Interfaces internas entregues no pacote 2

As novas interfaces não alteram os tipos do pacote 1 nem o seam `expand()`.
A revisão técnica do PR #32 conferiu o handoff aos pacotes 3, 4 e 6 descrito
abaixo; ela não representa aprovação nominal desses responsáveis. A confirmação
deles continua registrada como acompanhamento no [pacote 2](work/02-definitions.md#handoff-e-revisão).

### Captura e representação

`DefinitionCapture::capture(std::span<const SourceLine> lines, std::size_t beginIndex)`
recebe linhas já escaneadas e um índice **iniciado em zero** para `MCDEFN`.
Retorna `DefinitionCaptureResult`, variante entre:

- `CapturedDefinition { definition, nextLine }`: definição própria e índice da
  primeira linha após o `MCEND` correspondente, no mesmo span. Só o trecho
  solicitado é consumido; linhas posteriores não são validadas.
- `MacroDiagnostic`: primeira falha, sem definição parcial. A cadeia `calls`
  fica vazia; o motor acrescentará o contexto quando capturar linhas geradas.

`MacroDefinition` é construída pela captura e expõe somente consultas const:
`begin()`, `prototype()`, `body()`, `end()` e `name()`. Os quatro primeiros
retornam, respectivamente, a linha de abertura, a linha do protótipo,
o vetor de linhas entre protótipo e fechamento, e a linha de fechamento.
`name()` retorna o `SourceField` da operação do protótipo, com grafia e posição.
Todos os textos, campos, números físicos, espaços finais e `terminated` são
preservados e pertencem à definição, independentemente da fonte e do scanner.

A busca do protótipo ignora somente linhas vazias/comentários. Essas linhas
entre a abertura e o protótipo não integram o corpo. A captura verifica a
presença da operação e o identificador do **nome de macro**; reutiliza
`SymbolTable::isValidIdentifier` e `isReservedName`. `MCDEFN` e `MCEND` também
não podem nomear macros: se forem a próxima operação ativa, indicam protótipo
ausente. Não se duplicam mnemônicos/diretivas do montador.

Para o **pacote 3**, `prototype()` mantém rótulo formal, operando completo,
comentário, `openQuote` e posições sem interpretar formais, vírgulas,
repetições ou `NOME=valor`. Captura bem-sucedida não certifica validade dos
formais. Para o **pacote 4**, `body()` conserva as definições internas inteiras,
inclusive delimitadores, protótipos e comentários, sem substituir parâmetros.
O contador reconhece somente diretivas no campo de operação, valida campos
dos delimitadores em todos os níveis e termina apenas ao voltar a zero.
Os protótipos internos ficam brutos; serão identificados pelos consumidores
de escopo e validados quando suas definições forem capturadas individualmente.

Comentários estruturais externos continuam disponíveis como metadados nas
linhas `begin()`, `prototype()` e `end()`, mas não entram no corpo. Comentários
estruturais internos permanecem no esqueleto para preservar suas posições;
o motor deve aplicar a política de não emissão quando os consumir como
estrutura. Comentários e linhas vazias comuns do corpo permanecem em ordem.
Esta interface não renderiza nem emite texto.

| Código da captura | Posição apontada |
| --- | --- |
| `MissingPrototype` | Operação do `MCDEFN` quando não há próxima linha ativa ou ela é um delimitador; para índice fora da entrada, linha física após a última (1:1 em entrada vazia) |
| `InvalidPrototype` | Início da operação com nome inválido; rótulo se falta operação; primeiro campo indevido (rótulo antes de operando) em `MCDEFN`/`MCEND` de qualquer nível |
| `ReservedName` | Operação do protótipo com nome reservado pelo montador |
| `UnexpectedEnd` | Operação do `MCEND` recebido como início de captura, sem abertura |
| `UnterminatedDefinition` | Operação do `MCDEFN` externo cujo nível não foi fechado |

Um início de captura que não seja delimitador recebe `InvalidPrototype` na
operação, ou no rótulo/coluna 1 se não houver operação. Não se acrescentaram
códigos ao enum compartilhado. Testes conferem código e posição, sem usar
mensagens como identificadores.

### Tabela e versões

`MacroTable::define(MacroDefinition)` instala explicitamente uma definição
capturada, copiando/movendo os dados para uma versão imutável própria.
`find(std::string_view)` retorna `MacroTable::Definition`, um
`std::shared_ptr<const MacroDefinition>`, ou `nullptr` se ausente. A chave é
normalizada por ASCII com `SymbolTable::normalizeName`; grafia e posições da
definição permanecem originais. A construção restrita de `MacroDefinition`
impede instalar nomes não validados pela captura.

Para o **pacote 6**, captura e instalação são ações distintas. Capturar ou
instalar a externa nunca instala suas internas. O motor decide quando fazer
cada operação e retém o handle retornado por `find()` durante uma expansão.
Redefinir o mesmo nome, mesmo com corpo maior, menor ou vazio, troca apenas o
handle ativo da tabela. Handles anteriores mantêm seu conteúdo e permanecem
válidos inclusive depois da destruição da tabela. Não há estado global.

`sicxe_macros` passa a ligar **privadamente** `sicxe_assembler` para as funções
de nomes de `SymbolTable`. Seus headers não expõem tipos do montador; não se
chamam parser, montagem, VM, Qt ou I/O. A implementação do scanner do pacote 1
continua independente do montador.

## Interfaces internas entregues no pacote 3

As novas interfaces em `MacroArguments.hpp` não alteram os tipos dos pacotes 1
e 2 nem o seam `expand()`. A revisão técnica do PR #33 conferiu os dados
necessários aos pacotes 4 e 6. Ela não representa aprovação nominal desses
responsáveis; essa confirmação permanece no [handoff](work/03-arguments.md#handoff-e-revisão).

### Protótipo, chamada e vinculação posicional

- `MacroFormal { kind, name, key }`: classifica o parâmetro formal como
  `MacroFormalKind::Label` ou `MacroFormalKind::Positional`, preserva o
  `SourceField` original (`name`) e guarda a chave normalizada em maiúsculas
  ASCII (`key`).
- `MacroPrototype { macroName, macroKey, labelFormal, positionalFormals, formals }`:
  representa o protótipo validado. `formals` mantém a ordem canônica do
  protótipo (`labelFormal` primeiro, quando declarado, seguido de
  `positionalFormals`). `findFormal(std::string_view)` retorna o índice
  baseado em zero em `formals` sem distinção de caixa, ou `std::nullopt`.
- `MacroCall { macroName, macroKey, label, arguments }`: representa uma linha de
  chamada validada sintaticamente, com rótulo opcional e argumentos posicionais
  divididos por vírgulas fora de aspas simples e sem espaços horizontais nas
  extremidades.
- `BoundMacroArgument { formal, value, position }`: associa cada `MacroFormal`
  ao texto vinculado (`value`) e à posição física na chamada (`position`, ou
  `std::nullopt` quando o rótulo ou argumento final foi omitido).
- `BoundMacroArguments { macroName, labelValue, positionalValues, values, arguments }`:
  lista ordenada de valores e vínculos na mesma ordem de `MacroPrototype::formals`,
  além de acesso direto a `labelValue`, `positionalValues` e consulta sem
  distinção de caixa por `find(std::string_view)`.

`MacroArguments` oferece:

- `parsePrototype(const SourceLine&)` e `parsePrototype(const MacroDefinition&)`,
  retornando `MacroPrototypeResult` (`std::variant<MacroPrototype, MacroDiagnostic>`);
- `parseCall(const SourceLine&)`, retornando `MacroCallResult`
  (`std::variant<MacroCall, MacroDiagnostic>`);
- `bind(const MacroPrototype&, const MacroCall&)`,
  `bind(const MacroPrototype&, const SourceLine&)`,
  `bind(const SourceLine&, const SourceLine&)` e
  `bind(const MacroDefinition&, const SourceLine&)`, retornando
  `MacroArgumentBindResult` (`std::variant<BoundMacroArguments, MacroDiagnostic>`).

O motor seleciona chamadas pela tabela de macros **já ativas**, antes de
invocar `parseCall()`/`bind()`. O parser de chamadas não consulta nomes
reservados nem classifica operações SIC/XE; a verificação de nome reservado
permanece na definição, em `parsePrototype()`. Linhas comuns não passam pelo
parser de chamadas para decidir se são instruções válidas.

`bind()` recebe protótipos e chamadas produzidos pelos parsers acima. Esses
resultados e os vínculos são snapshots próprios: os consumidores devem
tratá-los como somente leitura, sem editar separadamente as coleções
derivadas (`formals`/`positionalFormals`, `values`/`arguments`/`positionalValues`)
ou suas chaves normalizadas. A coerência e a ordem são mantidas pelos produtores.

Para o **pacote 4**, `MacroPrototype::formals` e `BoundMacroArguments::values`
compartilham a mesma indexação ordenada, evitando reprocessar o protótipo ou a
chamada durante a substituição. Para o **pacote 6**, a cadeia `calls` do
`MacroDiagnostic` permanece vazia para ser preenchida pelo motor quando a falha
ocorrer dentro de uma expansão.

| Código em argumentos | Posição apontada |
| --- | --- |
| `InvalidPrototype` | Operação ausente/inválida ou diretiva; início do rótulo formal inválido; início do parâmetro formal vazio, malformado, `NOME=valor` ou repetido; aspa simples de abertura não fechada no protótipo |
| `ReservedName` | Operação do protótipo com nome reservado pelo montador |
| `InvalidArgument` | Linha de chamada inválida ou nome divergente do protótipo; rótulo na chamada quando o protótipo não declara rótulo formal; início de rótulo ou argumento com `NOME=valor`; primeiro argumento posicional em excesso; aspa simples de abertura não fechada na chamada |

## Interfaces internas entregues no pacote 4

`MacroTemplate.hpp` e `ParameterSubstitution.hpp` acrescentam a representação
de escopos e a substituição. Não alteram os tipos dos pacotes 1–3, a sintaxe
acordada ou `expand()`. O módulo não instala definições, escolhe chamadas,
gera serial, controla recursão, monta instruções ou faz I/O.

### Compilação e dados imutáveis

`MacroTemplate::fromDefinition(const MacroDefinition&)` devolve
`MacroTemplateResult::value`, variante entre `MacroTemplate` e
`MacroDiagnostic`. Valida os protótipos pelo pacote 3, inclusive os internos
necessários à análise de escopo. Falhas propagam código/posição, sem template
parcial; `calls` fica vazio para o motor contextualizar. Capturar/compilar
uma definição interna não a torna visível na tabela.

`root()` expõe `const TemplateScope&`. O escopo contém `prototype`, linhas
estruturais `beginLine`/`prototypeLine`/`endLine` e `elements` em ordem.
Cada `TemplateElement` é uma `TemplateLine` ou um
`shared_ptr<const TemplateScope>` interno; cópias compartilham apenas escopos
imutáveis. Fonte, protótipos e valores capturados possuem seus dados.

Uma `TemplateLine` guarda `source` e referências `ParameterReference`:
`field` (`Label`/`Operand`), `scopeDepth` (0 atual, 1 pai, ...), `formalIndex`
na ordem do pacote 3 e offsets de byte `begin`/`length` no campo original.
A busca usa `findFormal()` do pacote 3, do escopo mais próximo ao mais externo.
Um formal interno sombreia o externo de mesmo nome; não se redescobrem
referências depois de inserir os argumentos reais.

Somente tokens inteiros em rótulo/operando são referências. Operação,
comentários e constantes entre aspas, inclusive os prefixos `C`/`X`, ficam
intactos. Tokens numéricos, identificadores malformados e nomes `NOME.SER`
são opacos: não substituir um sufixo dentro de `0xAB`, `_VALUE` ou `.SER`.
Isso não valida assembly nem implementa serial.
Não se classifica `,X` como indexação: sem formal `X`, o texto literal fica
intacto; com formal `X`, sua ocorrência é substituída como qualquer outro
formal, inclusive em argumentos de chamadas internas. A vírgula permanece.

### Instanciação e pré-condições

`ParameterSubstitution::instantiate(template, arguments)` recebe um template
válido e `BoundMacroArguments` produzido por `bind()` para seu `root().prototype`.
Não montar/modificar coleções derivadas manualmente nem usar argumentos de
outro protótipo. Os valores são os campos de uma linha física escaneada, sem
terminadores de linha embutidos. A operação não modifica as entradas.

O retorno `SubstitutionResult` possui:

- `lines`: corpo renderizado como `vector<SourceLine>`, sem abertura,
  protótipo ou fechamento da macro chamada. Preserva comentários, espaços,
  ordem e definições internas estruturais; não expande chamadas.
- `definitions`: somente as definições internas **diretas**, em ordem, com
  `PreparedMacroDefinition { beginLine, nextLine, macroTemplate }`.
  O intervalo `[beginLine, nextLine)` é iniciado em zero e aponta para as
  linhas renderizadas daquela definição em `lines`. Seus descendentes serão
  preparados quando ela própria for chamada, não instalados antecipadamente.

Cada template preparado mantém referências originais e cópias dos vínculos
externos, do pai ao ancestral mais distante. Seus próprios formais continuam
sem vínculo. Ex.: externa `P` recebe texto `Q`; uma interna declara formal
`Q` e contém `ADD P`/`LDA Q`. Ao chamar a interna com `7`, o resultado é
`ADD Q`/`LDA 7`, não `ADD 7`/`LDA 7`. Texto inserido não vira um novo formal.
Templates preparados podem ser usados depois de destruir a fonte/resultado
externo e por chamadas independentes, sem estado global.

As substituições são aplicadas da direita para a esquerda nos offsets
originais. A linha renderizada passa novamente pelo scanner lexical do
pacote 1 para manter `kind`, opcionais vazios, aspas e colunas coerentes com
`text`. Isso não é nova passagem do processador de macros nem validação de
assembly. `number` e a linha física das posições continuam os da definição
original; colunas são de byte no texto renderizado. `terminated` é preservado.
O pacote 5 continua responsável pelo mapa de origem, não por restaurar campos
incoerentes. Rótulo/operando removido fica ausente, não presente com texto vazio.

`ParameterSubstitution::substitute(template, arguments)` é conveniência que
retorna somente `instantiate(...).lines`. Serve para renderização textual;
**não é a interface para o motor instalar definições internas**, pois descarta
os templates preparados.

### Handoff para os pacotes 5 e 6

O motor consome `lines` em ordem. Quando chegar ao `beginLine` de uma definição
preparada, pode capturar suas linhas com o pacote 2 e instalar a versão naquele
instante. Deve associar `macroTemplate` à **versão específica** instalada,
preservando templates antigos durante redefinições, não apenas indexá-los pelo
nome global. Nas chamadas seguintes, vincular os argumentos ao protótipo
daquela versão e usar o template preparado; **não recompilar o corpo das
linhas já substituídas**, pois isso reinterpretaria valores como formais.
O pacote 4 não faz instalação nem controla essa ordem temporal.

O contexto do pacote 5 atua nas linhas renderizadas que o motor efetivamente
emite/expande na chamada corrente, preservando os templates imutáveis e suas
capturas. Os intervalos de definição interna são estrutura, não emissão final:
seu tratamento deve respeitar a chamada em que aquela interna for expandida.
O pacote 4 deixa `.SER` intacto e conserva números físicos para esse contexto;
não escolhe serial ou cadeia de chamadas. A revisão técnica conferiu esse
handoff; não representa aprovação nominal dos responsáveis 5/6.

## Política de comentários na expansão

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
