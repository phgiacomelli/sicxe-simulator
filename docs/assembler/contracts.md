# Contratos do montador SIC/XE

Estado: fonte e tipos mínimos implementados no pacote 1; símbolos e valores
de operandos implementados no pacote 3; catálogo, operandos de instrução e
codificação de formato 2 implementados no pacote 2; passagem 1 e contador de
localização (`LocatedLine` e `PassOne`) implementados no pacote 4. `SymbolTable`
consulta o catálogo como fonte única de mnemônicos reservados, mantendo as oito
diretivas separadas. Dados e reservas estão implementados no pacote 5 e
integrados ao contador; a codificação de instruções dos formatos 2/3/4 foi
implementada no pacote 6. A segunda passagem, writers em memória e CLI estão
implementados no pacote 7, com validação local, revisão técnica e CI verde no
[PR #30](https://github.com/phgiacomelli/sicxe-simulator/pull/30).
Mudanças de comportamento e divisão de ownership exigem acordo dos consumidores.

## Fonte de verdade

`PRIMEIRO_TRABALHO_SIC_XE.pdf`, distribuído fora do Git, define o recorte
arquitetural. A VM existente, documentada em [decoder](../vm/decoder.md) e
[endereçamento](../vm/addressing.md), concretiza os bytes aceitos. Os
slides `Montador.pdf`, também distribuídos fora do Git, fornecem o método
de duas passagens. Onde os PDFs não especificam uma sintaxe de assembly,
as regras abaixo são **convenções mínimas do grupo**, não novos requisitos
do professor. Nenhuma convenção pode gerar bytes que o `Decoder` rejeite.

## Interface externa e intermediário

`assembler::assemble(std::string_view sourceText)`, em `Assembler.hpp`, devolve sucesso com
endereço `START`, PC de entrada, bytes finais, linhas da listagem e tabela
de símbolos; ou falha com diagnósticos estruturados. Ela não abre arquivos,
não recebe `Machine`/widgets e não mantém estado entre chamadas. Erro em
qualquer passagem impede publicação de bytes ou arquivos de sucesso.

```text
sourceText → SourceParseResult/SourceLine → LocatedLine → AssemblyResult
           parser implementado            passagem 1    passagem 2/saída
```

Cada linha preserva texto original, número físico começando em 1 e colunas
dos campos. Comentários e linhas vazias não avançam o contador, mas ficam
na listagem. `LocatedLine` fixa endereço e tamanho na passagem 1; a segunda
não pode alterá-los. Diagnósticos têm código estável, linha, coluna e
mensagem humana; testes usam código e posição, não o texto da mensagem.

Os tipos do pacote 1 vivem no namespace `assembler`:

- `SourceParser::parse(std::string_view)` retorna `SourceParseResult`:
  `lines`, `diagnostics` e `succeeded()` (sucesso sintático somente).
- `SourceLine` preserva `number`, `text`, `kind`, `statement` opcional e
  `comment` opcional. `ParsedStatement` guarda `label` opcional,
  `operation` e `operand` opcional. `SourceIdentifier` tem `source` e
  `key` normalizada; `SourceField` tem `text` original e `position`.
- `SourcePosition` usa linha e coluna de **byte** iniciadas em 1, inclusive
  para tabs. Resultados possuem suas strings e não dependem do texto de
  entrada após a chamada. Uma linha inválida não expõe declaração parcial.
- `AssemblyDiagnostic` tem código tipado, posição e mensagem.
- `AssemblyResult` é a variante entre `AssemblySuccess` (endereços,
  bytes, listagem e símbolos) e `AssemblyFailure` (somente diagnósticos).
  Esses tipos são dados; `assemble()`/`PassTwo` e `RawImageWriter` produzem e
  validam o resultado final, sem modificar os contratos anteriores.

As formas C++ da fonte, seus erros e a recuperação por linha estão em
[fonte e contratos mínimos](source.md). O pacote 3 entrega `SymbolTable`
e `OperandValue`, documentados em [símbolos](symbols.md). Seus diagnósticos
tipados são convertidos para `AssemblyDiagnostic` antes de compor
`AssemblyFailure`. O pacote 2 acrescenta `InstructionSpec`,
`InstructionOperand` e `InstructionDiagnostic`, documentados em [ISA](isa.md),
sem alterar campos ou assinaturas dos tipos existentes. O token do valor de
memória é preservado como `SourceField` para o pacote 3/6; apenas contagens de
shift são convertidas, usando `parseKnownInteger` existente. O pacote 4 entrega
`LocatedLine`, `PassOne` e `PassOneResult`, documentados em
[passagem 1](pass-one.md). O pacote 5 entrega `measureData` e
`encodeData`, com dados ou reservas explícitas, conforme [data.md](data.md),
sem alterar as interfaces anteriores.

O pacote 6 entrega `encodeInstruction(locatedLine, symbols, baseState)` e
`BaseState`, com resultado e diagnósticos em [encoding.md](encoding.md).
`PassTwo::run(const PassOneResult&)`, do pacote 7, mantém `BASE`/`NOBASE` em
ordem, resolve a base com a tabela final e delega instruções/dados aos módulos
existentes. Confere os tamanhos contra `LocatedLine` sem alterar o layout.
Erros de qualquer etapa impedem um `AssemblySuccess`, inclusive em falha tardia.
Os novos diagnósticos `EmptyProgram` e `LayoutMismatch` identificam imagem
vazia e divergência do intermediário. APIs e exemplos estão em [uso](usage.md).

## Dialeto mínimo da fonte

- Mnemônicos, registradores e rótulos usam ASCII e comparação sem distinção
  de maiúsculas/minúsculas. Rótulos seguem `[A-Za-z][A-Za-z0-9_]*` e não
  podem colidir com mnemônico/diretiva. A grafia original é preservada na
  listagem. Rótulo opcional começa na primeira coluna; linha sem rótulo
  começa com espaço ou tab. `.` inicia comentário de linha inteira na
  primeira posição não branca; `;` inicia comentário inline fora de aspas.
  O parser aceita LF, CRLF e CR e preserva linhas vazias; não cria uma
  linha fictícia após o terminador final. O operando pode conter espaços
  dentro de aspas e junto de vírgulas; campos extras são erro de sintaxe.
- `START endereço` é opcional apenas na primeira linha ativa (padrão 0) e
  pode ter rótulo que nomeia o início. `END [rótulo]` é obrigatório, encerra
  o programa e fixa o PC inicial; sem operando, o PC é `START`. Texto ativo
  depois de `END` é erro. `END` aceita apenas rótulo definido, não expressão.
- `BYTE C'...'` usa caracteres ASCII; `BYTE X'...'` usa pares de dígitos
  hexadecimais. `WORD valor` ocupa 3 bytes big-endian e aceita inteiro ou
  rótulo. Inteiros de `WORD` aceitos: `-0x800000..0xFFFFFF`, armazenados nos
  24 bits inferiores. `RESB n` reserva `n` bytes; `RESW n` reserva `3n`
  bytes. `n` é inteiro não negativo, sem expressão. Reservas viram zeros
  apenas no `.bin` bruto, para preservar os endereços seguintes.
- `BASE rótulo|endereço` e `NOBASE` não emitem bytes e controlam apenas a
  escolha do deslocamento base na passagem 2. `BASE` **não escreve B** em
  tempo de execução: o assembly deve carregá-lo antes de usar instruções
  base-relative.
- Inteiros usam decimal ou prefixo `0x` hexadecimal; rótulos são o único
  operando simbólico. Não há aritmética de rótulos ou expressões. `#` é
  imediato, `@` indireto, `,X` indexado somente no modo simples e `+`
  solicita formato 4 em instrução de memória que o suporte. Imediato
  numérico negativo é inválido: o `AddressResolver` da VM não estende o
  sinal do campo imediato.
- Formato 2 usa `A,X,L,B,S,T,PC,SW`; `F` e códigos desconhecidos são
  inválidos. `CLEAR`/`TIXR` usam um registrador; `SHIFTL`/`SHIFTR` usam
  registrador e contagem de 1 a 16, codificada como `n-1`; os demais usam
  dois registradores `r1,r2`. `RSUB` não tem operando nem forma `+`.

Essas diretivas são **só o mínimo de escrita/posicionamento de bytes para
esta etapa**. O primeiro PDF não prescreve seus nomes. `EQU`, `ORG`,
`LTORG`, literal `=...`, `CSECT`, `USE`, `EXTDEF`, `EXTREF`, `SET`,
`INTDEF`, `INTUSE`, `L()`/`A()` e qualquer construção não listada aqui não
fazem parte do dialeto. Rejeitar operação ou sintaxe desconhecida; não
aceitar silenciosamente.

## Endereços e codificação

O contador mede **bytes**; cada palavra da VM tem 3 bytes big-endian. O
espaço endereçável é `0x00000..0xFFFFF`, e a soma de início + extensão
não pode exceder `0x100000`. Passagem 1 registra rótulos locais,
endereços e tamanhos (`2`, `3` ou `4` bytes para instruções, conforme
formato escolhido na fonte). Referência futura recebe o endereço da
tabela na passagem 2. Símbolo duplicado/indefinido, opcode desconhecido,
faixa excedida ou operando inválido são diagnósticos; não usar zero
provisório como endereço final.

O catálogo contém exatamente as 38 instruções aceitas pela VM. Formato 2
codifica opcode e nibbles. Para alvo simbólico de formato 3, tentar
PC-relative com deslocamento `[-2048,2047]`, depois base-relative
`[0,4095]` se `BASE` estiver ativo. Para operando numérico absoluto de
até 12 bits, usar campo direto `b=p=0`. Se nada couber, falhar e indicar
que a fonte precisa de `+` quando aplicável: não promover formato
automaticamente depois da passagem 1. Formato 4 usa endereço absoluto
de 20 bits com `b=p=0`; rótulos são resolvidos para o endereço fixo do
programa. Não emitir registros de relocação.

O montador emite codificação XE de formatos 2, 3 e 4. Não precisa de
formato 1 (todos os opcodes desse formato estão fora das 38 instruções)
nem de uma sintaxe inventada para emitir o modo SIC legado `n=i=0`.
A compatibilidade é verificada decodificando bytes produzidos e executando
programas de aceitação na VM.

## Saídas e falhas

O resultado de sucesso contém uma **imagem binária fixa** do endereço
`START` até o último byte emitido/reservado, com zero nas reservas, e
uma listagem de linha, endereço, bytes ou reserva. Não exportar imagem
vazia. O CLI grava `.bin` e `.lst`, reporta endereço de carga e PC
inicial para a VM e retorna código não zero em erro. A VM deve carregar
o `.bin` **no mesmo `START`**; não há relocação. Não criar `.obj` com
`H/T/M/E`, `M` ou loader: nem o primeiro trabalho nem os slides definem
esse contrato de arquivo para a SIC/XE do projeto.

`RawImageWriter` valida e constrói bytes em memória; `ListingWriter` formata
listagem e diagnósticos. O I/O fica em `sicxe_assembler_cli`, separado da
biblioteca pura `sicxe_assembler`. A listagem inclui texto original e símbolos
em ordem de definição; erros são reportados no stderr, sem `.lst` de sucesso.

`SICXEAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]` não sobrescreve saídas,
que devem ser distintas e ter diretórios existentes. Grava os dois conteúdos
temporários antes de publicar, usando hard links (suporte obrigatório no sistema
de arquivos). Desfaz o binário recém-publicado se falhar ao publicar a listagem;
uma falha adicional de remoção é reportada como `RollbackFailed`, com caminho
para recuperação manual. Não há transação durável entre os dois arquivos.
Veja [política de saída e códigos da CLI](usage.md#política-dos-arquivos-de-saída).

## Mudanças de contrato

O pacote 1 materializa linha, diagnóstico e resultado mínimos. Os pacotes
2–6 podem adicionar dados privados às suas implementações; mudança em
campos públicos, gramática ou regras acima requer revisão dos consumidores,
teste e atualização deste documento no mesmo PR. O pacote 7 coordena pelas
interfaces, sem duplicar parser, símbolos ou codificador.
