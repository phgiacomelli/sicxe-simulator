# Pacote 5 — rótulos locais e rastreabilidade da expansão

Responsável: um integrante. Estado: **planejado**. Depende dos pacotes 1
e 4. Ler [contratos](../contracts.md) e as alternativas de rótulo dos slides.

## Entrega e ownership

Implementar a convenção explícita `.SER` dos slides. Cada **chamada
expandida** recebe um número serial determinístico; todas as ocorrências
de `NOME.SER` dessa chamada viram o mesmo identificador SIC/XE, por exemplo
`LOOP_M1`. Outra chamada recebe outro serial. O serial começa em 1, é decimal
e não tem preenchimento. Ele é global ao
processamento de um arquivo, inclusive chamadas aninhadas; não é global ao
processo nem compartilhado entre execuções de `expand()`.

O módulo atua sobre tokens de rótulo/referência após a substituição formal,
sem tocar comentários e literais entre aspas. Deve validar que o nome final
obedece à gramática do montador e não é reservado. Se houver colisão com um
rótulo normal presente na fonte ou com outro nome gerado, diagnosticar em
vez de criar assembly ambiguamente válido. O registro de nomes acompanha
as linhas **à medida que são emitidas**, inclusive rótulos comuns que
apareçam depois; não varrer a fonte antecipadamente. Se o contador de serial
esgotar sua representação, diagnosticar antes de produzir nome repetido.
Arquivos esperados:
`LocalLabelSerial.*` e `tests/macros/LocalLabelSerialTest.cpp`.

O mesmo contexto de expansão registra a origem de cada linha produzida:
linha física de definição, linha da chamada que a gerou e cadeia de chamadas
aninhadas. Entregar esse mapa ao resultado público definido no pacote 1,
para o integrador relacionar diagnósticos do montador à fonte original.
Não remapear diagnósticos do montador aqui; apenas guardar informação
determinística e suficiente para o pacote 7.

## Casos obrigatórios

- Definição com rótulo `LOOP.SER` e referências para ele, expandida duas
  vezes: dentro de cada expansão os nomes coincidem; entre expansões diferem.
- Chamada aninhada consome serial distinto, sem reiniciar contador.
- Variação de caixa, marcador em comentário/aspas, marcador sem prefixo
  válido, overflow e colisão com símbolo já existente.
- Rótulos comuns permanecem intactos; não renomear automaticamente símbolos
  definidos fora da macro.
- Mapa de origem para linha comum, linha de macro direta e linha de chamada
  aninhada; a origem continua válida após destruir o texto de entrada.

## Não fazer

Não mudar `SymbolTable`, parser ou semântica de rótulos do montador.
`.SER` desaparece antes de chamar `assembler::assemble`; não é uma diretiva
ou notação nova aceita pelo montador puro.

## Aceite do PR

Testes por saída observável e diagnósticos tipados; CMake/CTest e formatação
passam. Documentar claramente a política de serial e colisões em
[contratos](../contracts.md) para o pacote 6 integrar sem inferências.
