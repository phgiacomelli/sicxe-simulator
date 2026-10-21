# Pacote 3 — parâmetros formais e argumentos reais

Responsável: um integrante. Estado: **planejado**. Depende do pacote 1;
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
