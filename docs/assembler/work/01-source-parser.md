# Pacote 1 — Fonte, diagnósticos e contratos mínimos

Estado: implementado e validado localmente; revisão cruzada e CI remota
pendentes.

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
- [x] Parser e diagnósticos estruturados testados via interface pública.
- [x] `docs/assembler/source.md` documenta a gramática efetiva e os erros.
- [x] Build, formatação e CTest locais passam com e sem Qt.
- [ ] CI remota verde e revisão externa dos contratos.

## Registro do PR

Entregues `SourceParser.*`, `SourceLine.hpp`, `AssemblyDiagnostic.hpp` e
`AssemblyResult.hpp` no namespace `assembler`. `sicxe_assembler` é uma
biblioteca C++20 independente de Qt/VM; `AssemblerTests` testa sua interface.

Decisões implementadas:

- strings são possuídas pelo resultado, sem dependência da vida da fonte;
- linha/coluna começam em 1, coluna conta bytes e tab conta um byte;
- LF, CRLF e CR são aceitos sem linha extra artificial no fim;
- rótulos/operações possuem grafia original e chave ASCII normalizada;
- comentários, operandos e aspas preservam seus dados;
- o primeiro erro de cada linha gera diagnóstico e `Invalid`, sem
  declaração parcial; a análise continua nas linhas seguintes;
- falha da montagem é um tipo separado que contém apenas diagnósticos;
- campos semânticos de opcodes/valores permanecem para os próximos pacotes.

São 24 testes novos: 22 de parser e dois dos contratos de resultado.
Os testes completos foram executados em configurações limpas com Qt e
sem Qt, usando a fonte GoogleTest já disponível em `build/_deps`;
formatação e compilação do parser com warnings como erros também passam.
O resultado local da suíte completa está em [testes](../../testing.md#montador-pacote-1).

Não foi necessário alterar regras arquiteturais dos PDFs. Gramática e
decisões de fonte seguem [contratos](../contracts.md), com os tipos
efetivos em [source.md](../source.md). As interfaces das passagens,
codificadores, writers e CLI ainda não foram implementadas. Este trabalho
local não substitui revisão dos responsáveis dos pacotes 2, 4 e 7 nem
evidência de CI remota.
