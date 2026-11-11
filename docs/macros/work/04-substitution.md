# Pacote 4 — substituição de parâmetros com escopo aninhado

Responsável: um integrante. Estado: **implementado, revisado tecnicamente e
validado localmente**. Depende dos pacotes
1, 2 e 3. Ler [contratos](../contracts.md) e os exemplos de definições
aninhadas do PDF.

## Entrega e ownership

Representar ocorrências de parâmetros do esqueleto por referência estável
ao nível/posição do protótipo (ou modelo equivalente). Ao capturar uma
definição interna, empilhar seus formais; a ocorrência usa o formal visível
mais próximo. Ao produzir a definição interna durante expansão da externa,
substituir somente os formais externos. Os formais da interna permanecem
para quando ela própria for chamada. Isso evita substituir nomes iguais em
escopos diferentes por acidente.

Renderizar campos de rótulo e operando preservando instrução, diretiva,
marcadores `#`/`@`/`+`/`,X`, espaços e comentários. Não substituir
identificadores parciais (`A` não muda `DATA`), mnemônicos no campo de
operação, comentários nem conteúdo entre aspas simples. Uma substituição
vazia pode retirar o rótulo formal e deixar a linha recuada, válida como
instrução sem rótulo. Arquivos esperados: `MacroTemplate.*`,
`ParameterSubstitution.*` e testes em `tests/macros/`.

## Casos obrigatórios

- Parâmetro em rótulo, operando simples, imediato, indireto e indexado.
- Parâmetro repetido, nomes com prefixo comum e maiúsculas/minúsculas.
- Externa `SCALE RP` define interna com `SHIFTR RP`: ao chamar `SCALE 3`,
  `RP` vira `3`, mas os formais próprios da interna permanecem intactos.
- Formal interno sombreia formal externo de mesmo nome; um externo não
  sombreado ainda é substituído no corpo da interna.
- Valores vazios/omissos vindos do pacote 3 e preservação de aspas/
  comentários.

## Não fazer

Não instalar definições, controlar recursão, gerar serial `.SER`, fazer I/O
ou validar a instrução emitida contra a ISA. Essas responsabilidades são
dos pacotes 2, 5, 6, 7 e do montador.

## Aceite do PR

Testes exercitam entrada/saída do módulo de template, inclusive definição
aninhada completa; sem inspeção da representação privada. CMake/CTest e
formatação passam; contratos alterados são revisados pelos pacotes 5 e 6.

## Registro da implementação e revisão

`MacroTemplate.*` compila a árvore de escopos e as referências estáveis;
`ParameterSubstitution.*` renderiza o corpo e prepara os templates das
definições internas com capturas próprias. As formas C++, pré-condições,
posições e o fluxo obrigatório para o motor estão somente no
[contrato comum](../contracts.md#interfaces-internas-entregues-no-pacote-4).

A revisão da branch encontrou e corrigiu:

- Rótulo/operando esvaziado ainda presente no `SourceLine`, causando falsa
  chamada rotulada ou argumento excedente. A renderização reutiliza o scanner
  lexical e preserva os números físicos/terminadores originais.
- Correspondência parcial em números/identificadores malformados e em
  `.SER`. O reconhecimento agora usa tokens inteiros e consulta os formais
  pelo pacote 3, sem duplicar normalização.
- Tratamento especial de qualquer `,X` como indexação, impedindo substituir
  um formal `X` em chamadas de macro. A substituição não interpreta ISA.
- Perda de identidade de referências ao devolver definições internas só como
  texto. `instantiate()` acrescenta templates preparados com vínculos externos
  próprios; evita que o motor substitua novamente valores já inseridos.
- Escopos internos mutáveis através de um template const; agora são
  `shared_ptr<const TemplateScope>`. A formatação do código também foi corrigida.

Havia sete testes originais. Foram acrescentados 13, totalizando **20 testes**
de substituição por interfaces públicas. Quatro regressões de campos vazios,
fronteiras de tokens e `.SER` falharam antes dos ajustes e passaram depois.
Também são verificados formal `X`, posições/texto coerentes, literais,
comentários, escopo sombreado, `SHIFTR A,RP`, chamadas sucessivas em três
níveis, capturas independentes após destruir a fonte e falha tipada de protótipo.

Validação local: **479 testes sem Qt e 482 com Qt**, ambos em builds Release
novos; formatação, `git diff --check` e novos fontes com
`-Wall -Wextra -Wpedantic -Werror` aprovados. Comandos em
[testes](../../testing.md#macros-pacote-4).

### Handoff e próximos passos

- Pacote 5: `.SER` e origem continuam seus. Consumir linhas renderizadas,
  respeitar intervalos estruturais e não modificar templates/capturas.
- Pacote 6: usar `instantiate()`, associar o template preparado à versão
  instalada no instante correto e não reconstruí-lo do texto já renderizado.
  `substitute()` sozinho perde esse contexto e não deve instalar internas.
- Pacote 7: sem nova CLI ou arquivo; `expand()` continua somente declarada.

A revisão técnica conferiu padrões e requisitos do handoff. Não atribui
aprovação nominal aos responsáveis dos pacotes 5/6. A CI verde no PR continua
obrigatória antes do merge; seu resultado deve ser consultado no próprio PR.
