# Pacote 3 — parâmetros formais e argumentos reais

Responsável: um integrante. Estado: **implementado com validação local;
revisão cruzada e CI pendentes**. Depende do pacote 1;
pode evoluir em paralelo ao 2. Ler [contratos](../contracts.md).

## Entrega e ownership

Implementar o parsing do protótipo e da chamada e a vinculação dos argumentos
reais aos formais. O rótulo formal opcional do protótipo é vinculado ao rótulo
da chamada; sem formal de rótulo, chamada rotulada é erro. Os demais formais
aceitam **somente argumentos posicionais**; posição vazia ou argumento final
omitido vincula texto vazio. Rejeitar `NOME=valor` nesta entrega, tanto no
protótipo quanto na chamada: não há parâmetros por palavra-chave nem valores
padrão. Trabalhar com texto de argumento, sem interpretar expressão,
registrador, endereço ou opcode SIC/XE.

A interface interna devolve uma lista ordenada de valores por formal ou
diagnósticos tipados; a substituição do pacote 4 não deve repetir parsing.
Arquivos esperados: `MacroArguments.*` e `tests/macros/MacroArgumentsTest.cpp`.

## Casos obrigatórios

- Posicionais completos; posições vazias por vírgulas; menos argumentos que
  formais; omissões vinculam texto vazio.
- Rejeição de `NOME=valor` em protótipo e chamada e de argumentos em excesso.
- Rótulo formal preenchido, omitido e chamada rotulada sem formal.
- Nomes insensíveis a caixa, formal repetido e separadores dentro de aspas
  simples.
- Valores como `#7`, `@PTR`, `C'A,B'` e nomes de rótulo passam como texto.
  `BUFFER,X` é tratado como dois argumentos: a macro pode formar `,X` em
  seu próprio esqueleto, sem inventar agrupamento de argumentos.

## Não fazer

Não alterar gramática do montador, não avaliar valores, não renderizar linhas
e não tratar `.SER`. O pacote 2 é responsável pela captura da definição;
este módulo recebe o protótipo já identificado.

## Aceite do PR

Testes cobrem aceitação e diagnósticos por código/posição, não apenas texto
de mensagem. CMake/CTest e formatação passam. Registrar qualquer ajuste na
sintaxe acordada em [contratos](../contracts.md) antes do merge.

## Registro da implementação

### Arquivos e decisões

- `src/macros/MacroArguments.hpp/.cpp`: parsing de protótipo (`MacroPrototype`),
  parsing de chamada (`MacroCall`) e vinculação posicional (`BoundMacroArguments`)
  com diagnósticos tipados (`MacroDiagnostic`) e propriedade integral dos dados.
- `tests/macros/MacroArgumentsTest.cpp`: 12 testes registrados no `MacroTests`;
  fontes registrados em `sicxe_macros`.
- Reutilização privada de `SymbolTable::isValidIdentifier`, `isReservedName` e
  `normalizeName` em `MacroArguments.cpp`, sem expor headers do montador em
  `src/macros/` e sem modificar tipos existentes dos pacotes 1 e 2.

A forma C++ e as regras de posição/diagnóstico ficam no
[contrato comum](../contracts.md#interfaces-internas-entregues-no-pacote-3).
Não há avaliação de expressões, interpretação de registradores/endereços,
substituição no corpo, numeração `.SER`, motor ou I/O.

### Testes e validação local

Os 12 testes novos cobrem protótipos com e sem rótulo formal, vinculação
posicional completa, posições vazias por vírgulas, omissões finais, chamada
rotulada com e sem formal de rótulo, insensibilidade a caixa, formais
repetidos, rejeição de `NOME=valor` em protótipo e chamada, argumentos em
excesso, separadores `,`/`;`/`=` protegidos por aspas simples, aspas abertas,
passagem textual de `#7`/`@PTR`/`LOOP.SER`, divisão de `BUFFER,X` em dois
argumentos e duração dos dados (incluindo bytes NUL).

Builds novos passaram em **458 testes sem Qt (Debug)** e **461 com Qt
(Release)**, sem falhas ou casos ignorados. O alvo `format`, a verificação
`clang-format --dry-run --Werror` em todo o C++, `git diff --check` e a
compilação isolada dos novos fontes/header com avisos como erros passaram.
Comandos reproduzíveis em [testes](../../testing.md#macros-pacote-3).

### Handoff e revisão

- Pacote 4: consumir `MacroPrototype` (`formals`, `labelFormal`,
  `positionalFormals`, `findFormal`) e `BoundMacroArguments` (`values`,
  `labelValue`, `positionalValues`, `arguments`, `find`) pela ordem dos formais,
  sem repetir parsing de protótipo ou de chamada.
- Pacote 6: validar protótipos com `MacroArguments::parsePrototype` e vincular
  chamadas ativas com `MacroArguments::bind`, acrescentando a cadeia `calls`
  aos diagnósticos quando ocorrerem dentro de expansões.

As interfaces são aditivas; nenhum contrato dos pacotes 1 e 2 foi alterado. O
PR deve pedir alinhamento dos pacotes 4 e 6 antes do merge.

- [x] Parsing de protótipo, parsing de chamada e vinculação posicional implementados.
- [x] Testes pelas interfaces cobrindo casos obrigatórios, códigos e posições físicas.
- [x] Contratos, índice, roadmap e guia de testes atualizados.
- [x] Formatação, build e CTest locais com e sem Qt aprovados.
- [ ] Revisão/alinhamento com integrantes dos pacotes 4 e 6.
- [ ] CI remota verde e revisão de outro integrante antes do merge.
