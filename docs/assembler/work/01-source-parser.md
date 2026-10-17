# Pacote 1 — Fonte, diagnósticos e contratos mínimos

Estado: planejado. Responsável: a definir.

## Resultado esperado

Ler um texto assembly sem conhecer a VM, preservando linhas e posições, e
entregar uma representação de fonte sem ambiguidade aos demais pacotes.
Materializar no mesmo PR os tipos compartilhados mínimos descritos em
[contratos](../contracts.md). Este é o primeiro PR de integração, não um PR
separado apenas de contratos.

## Ownership

Arquivos novos sugeridos: `src/assembler/SourceParser.*`,
`src/assembler/SourceLine.hpp`, `src/assembler/AssemblyDiagnostic.hpp`,
`src/assembler/AssemblyResult.hpp`, `tests/assembler/SourceParserTest.cpp` e
`docs/assembler/source.md`. Pode ajustar os nomes mantendo uma interface
equivalente. Alterar `CMakeLists.txt` apenas para compilar/testar os próprios
arquivos; não mexer em `src/core/` nem na UI.

## Interface a entregar

- `parse(sourceText)` retorna linhas físicas em ordem e diagnósticos; nenhuma
  leitura de arquivo ocorre no parser.
- Cada linha distingue comentário/vazia/declaração, guarda texto original,
  número e colunas; declaração guarda rótulo opcional, operação e operandos
  brutos ou tokenizados sem perder aspas, vírgulas e prefixos.
- Um diagnóstico tipado guarda código, linha, coluna e mensagem; o resultado
  público da montagem separa sucesso de falha e nunca expõe saída parcial.
- Os tipos devem ser suficientes para uma passagem 1 anexar endereço/tamanho
  sem reparsear texto e para a listagem reconstruir a linha original.

## Regras de implementação

Aceitar comentário de linha com `.` e comentário `;` fora de aspas, espaços e
tabs, operandos com `C'...'`/`X'...'` contendo espaços, e operações com prefixo
`+`. Normalizar mnemônicos/rótulos para consulta sem descartar a grafia
original. Resolver a ambiguidade de rótulo pelo início da linha, conforme
[contratos](../contracts.md#dialeto-mínimo-da-fonte). Não validar símbolo existente,
opcode conhecido, valor numérico nem modo de endereçamento aqui: isso
pertence aos pacotes seguintes. A recuperação após linha inválida deve
permitir diagnosticar outra linha, mas nunca tratá-la como válida.

## Testes obrigatórios

- linha com e sem rótulo, comentário integral/inline, branco, CRLF e EOF sem
  nova linha;
- aspas com espaço, ponto-e-vírgula e vírgula; `+JSUB`, `#`, `@` e `,X`;
- colunas corretas para operador faltante, aspas abertas, rótulo inválido e
  campo extra;
- duas invocações independentes não compartilham estado;
- tipos públicos podem ser usados em testes mínimos dos pacotes 2–7.

## Fora do escopo

Não montar bytes, consultar tabela de símbolos, definir tamanhos ou produzir
arquivos. Não criar um parser de macros, múltiplas seções ou sintaxe das
instruções vermelhas.

## Critério de conclusão

- [ ] Contratos mínimos revisados pelos responsáveis dos pacotes 2, 4 e 7.
- [ ] Parser e diagnósticos estruturados testados via interface pública.
- [ ] `docs/assembler/source.md` documenta a gramática efetiva e os erros.
- [ ] Build, formatação, CTest e CI verdes; nenhuma mudança sem relação.

## Registro do PR

Preencher ao implementar: decisões específicas, divergências dos PDFs,
interfaces alteradas, testes executados, pendências e estado dos itens acima.
