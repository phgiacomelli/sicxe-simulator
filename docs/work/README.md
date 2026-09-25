# Pacotes de trabalho

Esta pasta detalha a divisão, o ownership e os critérios de conclusão de cada
pacote. Cada integrante deve ler [o guia de contribuição](../../CONTRIBUTING.md),
a [arquitetura](../architecture.md) e somente então o documento do seu pacote.

| Pacote | Responsabilidade | Dependência principal | Estado |
| --- | --- | --- | --- |
| [1](01-address-resolver.md) | Base arquitetural e resolução de operandos | Decoder concluído | Concluído |
| [2](02-data-operations.md) | Transferência de dados e lógica | Contratos do pacote 1 | Concluído |
| [3](03-arithmetic-operations.md) | Aritmética, comparações e shifts | Contratos do pacote 1 | Concluído |
| [4](04-machine-control.md) | Controle de fluxo e ciclo da máquina | Pacotes 1, 2 e 3 | Concluído |
| [5](05-qt-adapter.md) | Entrada de programas e adaptação Qt | Interface pública do pacote 4 | Concluído |
| [6](06-interface-acceptance.md) | Interface gráfica e aceitação | Pacotes 4 e 5 | Liberado pelo pacote 5 |

O [roadmap](../roadmap.md) é a fonte principal para o estado global. A coluna
`Estado` desta tabela apenas resume essa informação como ponto de entrada para
cada integrante e deve ser atualizada no mesmo PR que altera o roadmap.

## Regra de ownership

O ownership indica quem lidera alterações em uma área; não impede colaboração.
Uma mudança fora do ownership do pacote deve ser descrita no pull request e
revisada pelo integrante responsável pela área afetada.

## Contratos compartilhados

O pacote 1 entrega, antes do paralelismo dos pacotes 2, 3 e 4:

- identificação tipada de registradores de 24 bits;
- `ResolvedOperand` e `ResolveFault`;
- `ExecutionEffect` e `ExecuteFault`;
- regra para PC pós-fetch;
- representação e atualização do Condition Code.

Essas interfaces possuem testes de construção e invariantes, sem antecipar a
semântica das instruções. O contrato de efeitos e a regra do PC estão descritos
em [contratos de execução](../execution-contracts.md). Mudanças nesses tipos
devem ser combinadas entre os responsáveis pelos pacotes afetados.

## Atualização dos pacotes

Durante a implementação, cada responsável deve manter no seu documento:

- decisões específicas tomadas;
- divergências encontradas entre PDF e código;
- pendências que ficaram fora do PR;
- estado final dos critérios de conclusão.

Comandos de build, formato, commits e revisão permanecem centralizados em
`CONTRIBUTING.md` e não devem ser copiados para estes arquivos.
