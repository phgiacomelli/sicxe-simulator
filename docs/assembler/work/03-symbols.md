# Pacote 3 — Símbolos e valores de operandos

Estado: planejado. Responsável: a definir.

## Resultado esperado

Entregar a tabela de rótulos **locais** e a resolução de operandos que são
um rótulo ou um inteiro. A passagem 1 define posições; a passagem 2 resolve
referências anteriores e futuras sem reabrir a fonte. Nenhuma expressão
aritmética, símbolo externo ou atributo é interpretado.

## Dependências e ownership

Consumir linhas e diagnósticos do pacote 1. Arquivos sugeridos:
`src/assembler/SymbolTable.*`, `src/assembler/OperandValue.*`,
`tests/assembler/SymbolTableTest.cpp`,
`tests/assembler/OperandValueTest.cpp`, `docs/assembler/symbols.md`.
Pacote 4 decide **quando** um rótulo recebe o contador atual; este pacote
decide **como** definições e referências são armazenadas e consultadas.

Usar `assembler::SourcePosition` do pacote 1 nas definições/erros e a
chave normalizada do `SourceIdentifier` para nomes vindos da fonte.
`AssemblySymbol` é o registro final de saída; não é uma tabela de símbolos
operacional. Os contratos existentes estão em [source.md](../source.md).

## Interface a entregar

- `define(label, address, position)` insere rótulo uma vez e preserva linha
  de definição; redefinição retorna diagnóstico com as duas posições.
- `reference(label, position)` registra uso; `resolve(label, position)`
  devolve endereço definido ou diagnóstico de símbolo indefinido.
- `parseValue(token, position)` distingue inteiro decimal/`0x` hexadecimal
  de rótulo, sem avaliar operadores. O consumidor pode exigir inteiro já
  conhecido (`START`, reservas) ou permitir rótulo futuro (instrução,
  `WORD`, `END`, `BASE`).
- Expor tabela final em ordem determinística para diagnóstico e listagem.
  Não expor mapa mutável ao parser, encoder ou CLI.

## Comportamento obrigatório

Nomes seguem [contratos](../contracts.md#dialeto-mínimo-da-fonte), com
comparação sem distinção de caixa; preserva-se grafia/posição original
para mensagens. Um rótulo comum representa o endereço em **bytes**
atribuído pela passagem 1. Referência futura não recebe valor falso:
fica pendente até a definição e vira erro antes da emissão se ausente.
Inteiro inválido, overflow e token composto (`A+1`, `2*B`) produzem
diagnóstico; o consumidor decide se aceita sinal e qual é a faixa válida
para sua diretiva. Não criar um avaliador de expressões.

## Testes obrigatórios

- definição/uso anterior e futuro; várias referências ao mesmo rótulo;
- normalização de caixa, nome inválido, colisão com mnemônico/diretiva;
- duplicação com posições das duas definições e indefinição com posição
  do uso;
- decimal, hexadecimal, zero, limites e overflow;
- `A+1`, `A-B` e parênteses rejeitados; identificador nunca definido
  continua indefinido, sem mecanismo de referência externa;
- duas montagens sucessivas não compartilham símbolos.

## Fora do escopo

Não avançar contador, emitir bytes, avaliar `EQU`/`ORG`, classificar
absoluto/relativo ou criar referências cruzadas. Não atribuir semântica
de montador ao `RegisterFile` da VM.

## Critério de conclusão

- [ ] Passagens 1 e 2 consultam a mesma tabela por interface estável.
- [ ] Referências futuras, duplicados, indefinidos e números têm testes.
- [ ] `docs/assembler/symbols.md` documenta regras e diagnósticos.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Preencher ao implementar: decisões, divergências dos PDFs, interfaces
alteradas, testes e pendências.
