# Pacote 6 — motor do processador em uma passagem

Responsável: um integrante. Estado: **planejado**. Depende dos pacotes
1 a 5. Pode preparar testes de comportamento antes dos merges. Ler
[contratos](../contracts.md).

## Entrega e ownership

Implementar `macro::expand(sourceText)` juntando scanner, captura/tabela,
vinculação de argumentos, substituição e serial. O motor percorre **uma vez**
a fonte original; linhas produzidas por uma expansão entram em uma pilha/
fila de trabalho e são processadas imediatamente, antes da próxima linha
original. Não fazer uma primeira varredura para coletar todas as macros.

As transições são: copiar linha comum, capturar definição após `MCDEFN`,
expandir chamada cujo nome esteja ativo. Uma definição interna só entra na
tabela quando sua linha gerada é processada. Uma chamada dentro da
expansão é expandida recursivamente em ordem. Ao entrar em uma chamada,
registrar o nome/versão ativa na pilha; ciclo direto ou indireto vira
diagnóstico. Definir e testar um limite explícito de profundidade/linhas
geradas para evitar consumo ilimitado, sem emitir fonte truncada.

Arquivos esperados: `MacroProcessor.*` e `tests/macros/MacroProcessorTest.cpp`.
O resultado inclui texto final sem linhas `MCDEFN`/`MCEND` nem chamadas
reconhecidas e mapa de origem por linha. Falha devolve só diagnósticos.

## Casos obrigatórios

### Handoff recebido do pacote 4

Consumir `ParameterSubstitution::instantiate()` e seus templates preparados,
conforme o [contrato](../contracts.md#interfaces-internas-entregues-no-pacote-4).
Quando a leitura atingir `PreparedMacroDefinition::beginLine`, instalar aquela
versão, com o template correspondente, e avançar até `nextLine`. Não instalar
todas as internas antecipadamente nem usar só `substitute()` como entrada.
Uma versão redefinida não invalida o template/capturas da expansão já iniciada.
Não recompilar linhas renderizadas para redescobrir formais: texto inserido
por uma macro externa não pode virar formal de uma interna por coincidência
de nome. O pacote 4 prepara esse contexto; o motor controla sua visibilidade.

### Verificação

- Chamada comum; chamadas dentro de chamadas; ordem de expansão profunda.
- Definição aninhada produzida por expansão externa e chamada posterior da
  interna, incluindo parâmetro externo em corpo interno.
- Chamada a macro ainda não definida passa sem expansão; o montador a
  rejeita se a operação não for válida. Redefinição vale só dali em diante,
  inclusive durante expansões aninhadas.
- Delimitadores sem par, parâmetros inválidos, recursão direta/indireta,
  limite de expansão e falha tardia sem sucesso parcial.
- Linhas SIC/XE comuns e comentários passam, sem consulta ao catálogo para
  classificar operações desconhecidas. A consulta ao catálogo ocorre somente
  ao validar **nomes de macro na definição** (pacote 2). O montador valida
  todas as demais operações copiadas.

## Não fazer

Não abrir/gravar `MASMAPRG.ASM`, não chamar `assembler::assemble`, não
codificar instruções e não alterar a VM. O motor não é o módulo principal
integrador; o pacote 7 tem esse papel.

## Aceite do PR

Os testes chamam a interface pública `macro::expand`, não estados internos.
Build/CTest com e sem Qt e formatação passam. Documentar limites escolhidos,
ordem de processamento e diagnósticos em [contratos](../contracts.md).
