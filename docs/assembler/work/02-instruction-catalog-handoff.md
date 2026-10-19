# Pacote 2 — Registro da integração revisada

O roteiro de pendências da entrega inicial foi resolvido na revisão do
[PR #26](https://github.com/phgiacomelli/sicxe-simulator/pull/26). As regras
vigentes estão no [plano do pacote](02-instruction-catalog.md), nos
[contratos](../contracts.md) e na [ISA](../isa.md).

## Decisão e implementação

A [revisão aprovada do PR #24](https://github.com/phgiacomelli/sicxe-simulator/pull/24#pullrequestreview-5560213589)
registrou a integração de nomes reservados como responsabilidade do pacote 2.
Durante a revisão do PR #26, `SymbolTable` passou a consultar o catálogo como
fonte única dos 38 mnemônicos. A lista local contém apenas as oito diretivas.

Os caminhos `isReservedName`, `define`, `reference` e `resolve` usam a mesma
regra; `parseValue` a reutiliza por meio de `isReservedName`. A validação do
identificador precede a reserva: `+LDA` continua sendo nome inválido, enquanto
`A`, `B`, `X`, `TARGET`, `ADD_LOCAL` e nomes de instruções fora do recorte podem
ser símbolos. As assinaturas públicas e os códigos de erro foram preservados.

O catálogo permanece independente de `SymbolTable`, `OperandValue`, Qt e VM.
Não houve extensão para passagens, layout, PC/base, formatos 3/4 ou CLI.

## Evidências

- Testes cobrem todos os 38 mnemônicos em caixa alta/baixa/mista nos caminhos
  de símbolos e valores; as oito diretivas continuam ausentes do catálogo.
- Os testes verificam códigos, posições e ausência de definições/referências
  inseridas após falha, além da regressão da suíte existente.
- Builds limpos Release passaram em 345 testes sem Qt e 348 com Qt; formatação
  e `git diff --check` também foram verificados. Comandos em
  [testes](../../testing.md#montador-pacote-2).
- CI e revisão externa ficam registradas no PR #26 para o commit final.

Não há pendência de código conhecida dentro do escopo do pacote 2. A montagem
completa continua sendo responsabilidade dos pacotes 4–7.
