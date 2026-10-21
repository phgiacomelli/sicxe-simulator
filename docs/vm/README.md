# Primeira etapa — VM SIC/XE

Estado: **implementada**. A VM recebe bytes codificados, executa as 38
instruções do recorte do projeto e permite acompanhar o estado na interface.
Ela não interpreta fonte assembly nem expande macros. Veja o
[roadmap](../roadmap.md) para o estado das demais etapas.

## Guias e contratos

- [Arquitetura](architecture.md): módulos, responsabilidades e separação do
  montador.
- [Decoder](decoder.md) e [endereçamento](addressing.md): leitura dos bytes e
  resolução de operandos.
- [Contratos de execução](execution-contracts.md),
  [transferência e lógica](execution-data.md) e
  [aritmética](execution-arithmetic.md): efeitos das instruções.
- [Ciclo da máquina](machine-cycle.md): validação e confirmação do estado.
- [Entrada de programas](program-input.md): bytes, arquivos binários e
  adaptador Qt.
- [Demonstração](demo.md): roteiro de uso da primeira etapa.
- [Pacotes de trabalho](work/README.md): divisão histórica entre os seis
  integrantes da VM.

Os comandos de verificação ficam no [guia geral de testes](../testing.md).
