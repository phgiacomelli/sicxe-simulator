# Pacote 3 — Símbolos e valores de operandos

Estado: implementado e validado; integração com as passagens pendente.

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

- [x] Interface estável permite registrar na passagem 1 e consultar na passagem 2.
- [x] Referências futuras, duplicados, indefinidos e números têm testes.
- [x] `docs/assembler/symbols.md` documenta regras e diagnósticos.
- [x] Build, formatação e CTest locais passam com e sem Qt.
- [x] CI remota verde e revisão cruzada.

A integração efetiva com as passagens 1 e 2 pertence aos pacotes 4 e 7;
essas passagens ainda não existem neste PR.

## Registro do PR

Entregues `SymbolTable.*` e `OperandValue.*` no namespace `assembler`, com
testes em `SymbolTableTest.cpp` e `OperandValueTest.cpp` e documentação em
[symbols.md](../symbols.md). Nenhum contrato público existente do pacote 1
ou da VM foi alterado.

Decisões implementadas:

- `SymbolTable` armazena definições na ordem determinística de inserção e
  expõe apenas `const std::vector<AssemblySymbol>&`, sem expor mapa mutável;
- nomes seguem `[A-Za-z][A-Za-z0-9_]*` sem distinção de caixa e rejeitam
  colisão com os 38 mnemônicos da VM e as 8 diretivas do dialeto;
- redefinição preserva a primeira definição e retorna `SymbolDiagnostic`
  com `position` (nova ocorrência) e `previousDefinition` (primeira linha/coluna);
- `reference()` registra usos anteriores e futuros na ordem física sem atribuir
  endereço provisório falso; `unresolvedDiagnostics()` e `resolve()` reportam
  `UndefinedSymbol` com a posição do uso;
- `parseValue()` classifica inteiros decimais e hexadecimais (`0x`/`0X`) com
  detecção de overflow em 64 bits, distingue rótulos válidos e rejeita
  expressões aritméticas, sinais aplicados a símbolos e parênteses;
- `validateInteger()`, `parseKnownInteger()`, `referenceOperand()` e
  `resolveOperand()` permitem ao consumidor parametrizar aceitação de sinal,
  faixa numérica e permissão de símbolo futuro.
- diagnósticos de símbolos e operandos são convertidos para
  `AssemblyDiagnostic`, sem publicar bytes parciais; a grafia original da
  definição é preservada na mensagem de redefinição.
