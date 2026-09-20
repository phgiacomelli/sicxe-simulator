# Pacotes de trabalho

Esta pasta é a fonte principal para a divisão da implementação restante. Cada
integrante deve ler [o guia de contribuição](../../CONTRIBUTING.md), a
[arquitetura](../architecture.md) e somente então o documento do seu pacote.

| Pacote | Responsabilidade | Dependência principal |
| --- | --- | --- |
| [1](01-address-resolver.md) | Base arquitetural e resolução de operandos | Decoder concluído |
| [2](02-data-operations.md) | Transferência de dados e lógica | Contratos do pacote 1 |
| [3](03-arithmetic-operations.md) | Aritmética, comparações e shifts | Contratos do pacote 1 |
| [4](04-machine-control.md) | Controle de fluxo e ciclo da máquina | Pacotes 1, 2 e 3 |
| [5](05-qt-adapter.md) | Entrada de programas e adaptação Qt | Interface pública do pacote 4 |
| [6](06-interface-acceptance.md) | Interface gráfica e aceitação | Pacotes 4 e 5 |

## Regra de ownership

O ownership indica quem lidera alterações em uma área; não impede colaboração.
Uma mudança fora do ownership do pacote deve ser descrita no pull request e
revisada pelo integrante responsável pela área afetada.

## Contratos antes do paralelismo

Antes de os pacotes 2, 3 e 4 implementarem operações, os tipos abaixo devem ser
propostos em um PR pequeno e aprovados pelos responsáveis desses pacotes:

- identificação tipada de registradores de 24 bits;
- `ResolvedOperand` e `ResolveFault`;
- `ExecutionEffect` e `ExecuteFault`;
- regra para PC pós-fetch;
- representação e atualização do Condition Code.

O PR de contratos deve conter interfaces e testes de construção/invariantes,
sem antecipar a implementação das instruções.

## Atualização dos pacotes

Durante a implementação, cada responsável deve manter no seu documento:

- decisões específicas tomadas;
- divergências encontradas entre PDF e código;
- pendências que ficaram fora do PR;
- estado final dos critérios de conclusão.

Comandos de build, formato, commits e revisão permanecem centralizados em
`CONTRIBUTING.md` e não devem ser copiados para estes arquivos.
