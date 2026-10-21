# Pacote 4 — substituição de parâmetros com escopo aninhado

Responsável: um integrante. Estado: **planejado**. Depende dos pacotes
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
