# Codificação das instruções SIC/XE

O Pacote 6 está implementado em `src/assembler/InstructionEncoder.*`, no
namespace `assembler`. O módulo codifica as 38 instruções do catálogo: dez
de formato 2 por delegação, 27 de memória com formatos 3/4 e `RSUB` apenas
no formato 3. Não depende de Qt, registradores/memória da VM ou I/O.

## Interface e resultados

```cpp
using BaseState = std::optional<std::uint32_t>;

InstructionEncodingResult encodeInstruction(
    const LocatedLine& line,
    const SymbolTable& symbols,
    BaseState baseState);
```

`InstructionEncodingResult` é uma variante entre `EncodedInstruction`, que
possui `std::vector<std::uint8_t> bytes`, e `InstructionEncodingDiagnostic`.
Sucesso tem exatamente 2, 3 ou 4 bytes, conforme `line.sizeBytes`; falha não
possui bytes parciais. Chamadas são determinísticas, não modificam linha ou
símbolos e não retêm base ou referências entre chamadas.

Consumir somente linhas de um `PassOneResult` bem-sucedido. O encoder também
valida a fronteira pública para intermediários construídos manualmente:

- `SourceLine` deve ser uma declaração válida, com `statement` presente;
- categoria deve ser `Instruction`, sem `directive` associada;
- chave normalizada e grafia original da operação devem corresponder;
- mnemônico, opcode, formato, tamanho e indicação de extensão devem coincidir
  com o catálogo e o operando analisado;
- endereço e extensão devem caber em `0x00000..0xFFFFF`, sem wrap;
- base ativa deve estar dentro de 20 bits.

`findInstruction`, `parseInstructionOperand` e `encodeFormat2` vêm do Pacote 2.
O operando é obtido de `line.source`, sem reabrir arquivo ou chamar novamente
`SourceParser`. `parseValue`, `resolveOperand`, `validateInteger` e as faixas
numéricas vêm do Pacote 3. Endereços simbólicos são conferidos explicitamente,
pois `resolveOperand` aplica a faixa recebida somente a inteiros. Nenhum contrato
público anterior foi alterado.

## Estado de BASE e NOBASE

`std::nullopt` significa base desativada; `BaseState{0}` significa base ativa
no endereço zero. Não existe sentinela numérica para indicar ausência de base.
O chamador fornece o estado vigente para cada instrução. Base inválida é erro
mesmo em uma chamada cuja instrução não precisaria dela.

A futura passagem 2, do Pacote 7, deverá percorrer as linhas na ordem da fonte,
resolver o operando de `BASE` pela tabela final e limpar o estado em `NOBASE`.
Isso não pertence ao encoder: ele recebe somente instruções e uma base já
resolvida. `BASE` não carrega o registrador B; o programa deve fazê-lo antes de
executar uma instrução base-relative. O encoder não lê B ou X em tempo de montagem.

O estado explícito evita uma classe mutável com ordem de chamadas implícita e
permite testes independentes de cada instrução. Não foi criado um controlador
de passagem 2 para acompanhar diretivas neste pacote.

## Escolha do campo e montagem dos bytes

As regras do dialeto permanecem em [contratos](contracts.md). A implementação
segue esta ordem:

1. Formato 2 delega seus dois bytes ao codificador existente.
2. `RSUB` emite `4F 00 00`, com `n=i=1`, sem operando ou bits adicionais.
3. Formato 4 explícito (`+`) usa o alvo absoluto de 20 bits, `e=1`, `b=p=0`.
4. Número em formato 3 exige `Address12BitRange`, com `b=p=0`.
5. Símbolo em formato 3 tenta PC-relative, depois base-relative, ou falha.

O PC usado no cálculo é o endereço da instrução mais 3. A subtração é signed
de 64 bits, antes de qualquer conversão para o campo. Deslocamentos em
`[-2048,2047]` usam `p=1` e os 12 bits inferiores do complemento de dois.
Se o PC não alcançar o símbolo, uma base ativa pode usar deslocamento unsigned
`alvo - base` em `[0,4095]`, com `b=1`. PC tem preferência quando ambos cabem.

Números absolutos não tentam PC/base; símbolos não ganham fallback direto por
terem endereço baixo. Imediato simbólico também calcula PC/base: a VM recupera
o endereço final como valor imediato. Não há promoção automática para formato 4.
O diagnóstico de alcance sugere escrever `+` explicitamente.

`#` usa `n=0,i=1`, `@` usa `n=1,i=0`, e simples usa `n=i=1`.
`,X` ativa `x` apenas no modo simples. Prefixos, indexação, aridade,
registradores, sinais e expressões são validados pelos módulos existentes.
Imediato negativo, inclusive `-0`, é rejeitado. Endereços numéricos negativos
também são rejeitados pelo validador de faixas.

| Fonte e contexto | Bytes esperados |
| --- | --- |
| `LDA #5` | `01 00 05` |
| `LDA @0x123` | `02 01 23` |
| `LDA 0x123,X` | `03 81 23` |
| `LDA TARGET`, endereço `0x1000`, alvo `0x1010` | `03 20 0D` |
| `LDA TARGET`, endereço `0x1000`, alvo `0x1000` | `03 2F FD` |
| `LDA TARGET`, endereço `0x1000`, base `0x3000`, alvo `0x3100` | `03 41 00` |
| `+LDA #0x12345` | `01 11 23 45` |
| `RSUB` | `4F 00 00` |

## Diagnósticos

`InstructionEncodingDiagnostic` contém código próprio, `SourcePosition`, mensagem
e `cause`, uma variante de `monostate`, `InstructionDiagnostic` e
`OperandDiagnostic`. Causas preservam os códigos e posições dos Pacotes 2/3.
`toAssemblyDiagnostic` usa as conversões desses módulos quando há causa; não
foi necessário ampliar o enum compartilhado de diagnósticos.

| Código do encoder | Causa / posição | Conversão sem causa encapsulada |
| --- | --- | --- |
| `InvalidSourceLine` | Linha sem declaração válida; coluna 1 | `InvalidOperation` |
| `InvalidInstruction` | Categoria inválida ou diagnóstico do Pacote 2 | `InvalidOperation` |
| `InvalidOperand` | Diagnóstico numérico/simbólico do Pacote 3 | Conversão da causa |
| `LayoutMismatch` | Campos divergentes; operação | `InvalidOperation` |
| `InvalidBaseState` | Base acima de 20 bits; operação | `ValueOutOfRange` |
| `AddressOutOfRange` | Endereço/extensão da instrução ou endereço simbólico fora de 20 bits | `ValueOutOfRange` |
| `DisplacementOutOfRange` | Símbolo não alcançável por PC/base; token do valor | `ValueOutOfRange` |

Falhas numéricas de formato 3 preservam `OperandDiagnosticCode::ValueOutOfRange`
e acrescentam à mensagem a sugestão de `+`. Erros como `UndefinedSymbol`,
`IntegerOverflow`, `NegativeImmediate` e `InvalidIndexing` preservam seus códigos
originais na causa. Testes usam código/posição e bytes, não mensagens humanas.

A validação da estrutura e operação precede a resolução; o layout precede os
limites de endereço/base; esses limites precedem a emissão e resolução final.

## Compatibilidade e limites da execução

Testes cruzados ligam `sicxe_assembler` a `SICXECore`; a biblioteca de produção
continua independente. Os testes verificam os 38 opcodes e 540 combinações das
27 instruções de memória: quatro modos em cinco estratégias (direto, PC, base,
formato 4 simbólico e formato 4 numérico), com decoder e endereço resolvido.
Há ainda testes de deslocamentos negativos/extremos e dois programas executados
pela `Machine` a partir dos bytes gerados, com dados e sub-rotina, e com B
carregado explicitamente para acessar um alvo distante por base.

Uma instrução pode ocupar o último byte da memória e ser codificada/decodificada.
A `Machine` existente rejeita a execução quando o PC pós-fetch chega a
`0x100000`, antes de executar até mesmo um salto ou `RSUB`. O encoder mantém a
regra de layout já aceita por `PassOne`; um teste documenta essa distinção.
Não há garantia de que bytes decodificáveis possam executar em qualquer estado
de memória/registradores. A VM conserva suas validações de execução.

O pacote não oferece `assemble()`, `PassTwo`, writer, `.bin`, `.lst`, CLI,
relocação, formato SIC legado ou instruções adicionais. A composição das imagens
dos programas de teste é somente uma fixture em memória, sem API de saída.
