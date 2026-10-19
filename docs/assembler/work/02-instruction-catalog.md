# Pacote 2 — Catálogo da ISA, operandos e formato 2

Estado: implementado e validado localmente; coordenação de símbolos, CI remota
e revisão externa pendentes. Ainda não concluído para integração.

## Resultado esperado

Fornecer uma única tabela de instruções para o **montador** com opcode,
formatos permitidos e forma dos operandos das 38 instruções conhecidas pela
VM. Validar/normalizar operandos de instruções e codificar o formato 2, para
que a passagem 1 saiba tamanhos e a passagem 2 não duplique regras.

## Dependências e ownership

Consumir linhas/diagnósticos do pacote 1 e consultar, somente para
compatibilidade, `src/core/Instruction.hpp` e `docs/decoder.md`. Arquivos
sugeridos: `src/assembler/InstructionCatalog.*`,
`src/assembler/InstructionOperand.*`, `src/assembler/Format2Encoder.*`,
`tests/assembler/InstructionCatalogTest.cpp`,
`tests/assembler/Format2EncoderTest.cpp`, `docs/assembler/isa.md`.
Não mover a tabela da VM para o montador nem alterar o decoder para servir ao
parser: os dois módulos têm responsabilidades distintas. Testes cruzados
devem detectar divergência de opcodes e formatos.

O pacote 3 já usa uma lista local dos 38 mnemônicos para impedir rótulos
reservados. Ao entregar o catálogo, combinar com seu responsável a consulta
de `SymbolTable` ao catálogo como fonte única desses nomes; manter as 8
diretivas separadas, pois não são opcodes. Cobrir a integração com testes.

O pacote 1 já oferece `assembler::SourceLine` e `ParsedStatement`;
operação/rótulo possuem `SourceIdentifier::key` normalizada e `source`
original. Consumir esses valores após `SourceParseResult::succeeded()`,
preservando `SourcePosition` nos diagnósticos. Veja [source.md](../source.md).

## Interface a entregar

- Consulta `findInstruction(mnemonic)` retorna descritor imutável com opcode,
  tamanho e padrão de operandos; ausência é erro tipado.
- `parseInstructionOperand(spec, parsedLine)` devolve operando tipado:
  registrador(es), registrador/contagem, memória/imediato/indireto, indexação,
  valor numérico/rótulo ou ausência. Preservar o token do valor para o
  pacote 3; não analisar expressões.
- `encodeFormat2(spec, operand)` devolve dois bytes ou diagnóstico, sem
  consultar símbolos, VM, registradores ou estado global.
- Diretivas não são falsos opcodes; a passagem 1 as reconhece separadamente.

## Comportamento obrigatório

Cobrir exatamente dez opcodes de formato 2 e 28 de memória. `+` só vale para
os de memória que aceitam formato 4; `+RSUB` é inválido. `RSUB` usa formato 3
sem operando. Validar `A,X,L,B,S,T,PC,SW`; rejeitar `F` e registradores
desconhecidos. `CLEAR`/`TIXR`: um registrador e nibble baixo zero. Shifts:
contagem fonte 1–16 e nibble codificado 0–15. Restantes: dois registradores
na ordem `r1,r2`. Para operandos de memória, rejeitar `#... ,X`, `@... ,X`,
vírgulas sobrando, modos sem valor, `=...` e operadores aritméticos.
Deixar a resolução de números/rótulos aos pacotes 3/6.

## Testes obrigatórios

- tabela exata dos 38 mnemônicos/opcodes e formatos, incluindo o primeiro e o
  último de cada grupo;
- round-trip dos bytes de cada formato 2 no `Decoder` existente;
- operandos corretos/incorretos de cada família; `F`, número de registradores,
  `+` indevido, indexação inválida e shift 0/17;
- código de falha e localização, sem depender do texto da mensagem;
- consulta à tabela não modifica estado.

## Fora do escopo

Não resolver símbolos, alocar endereços, escolher PC/base, codificar formatos
3/4 ou aceitar instruções vermelhas. Não introduzir instruções que a VM não
executa para “completar” uma tabela externa.

## Critério de conclusão

- [x] Catálogo coincide com o recorte da VM, sem lista de opcodes vermelhos.
- [x] Formato 2 é utilizável diretamente pela passagem 2 e testado.
- [x] `docs/assembler/isa.md` registra operadores e formatos aceitos.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Implementação local na branch `feat/assembler-isa`, criada após atualizar
`main` com `git pull --ff-only` (base `7eefe8e`). O checkout estava limpo;
nenhum trabalho preexistente foi descartado. Não foi feito merge.

Entregues `InstructionCatalog.*`, `InstructionOperand.*` e `Format2Encoder.*`,
com testes próprios e [documentação da ISA](../isa.md). A alteração em
`CMakeLists.txt` registra os novos arquivos e separa `AssemblerDecoderTests`,
o único alvo novo que liga o montador ao núcleo da VM. Não houve mudança de
dependências, tipos existentes do parser/símbolos ou comportamento da VM.

Decisões:

- Tabela `constexpr` de 38 entradas, exposta por descritores constantes;
  dez opcodes de formato 2 e 28 do grupo de memória, com RSUB restrito ao 3.
- `InstructionOperand` conserva operação, tokens e posições da fonte e
  informa formato/tamanho; memória conserva valor como token, sem conversão
  numérica, resolução de símbolos ou endereçamento. Shifts reutilizam
  `parseKnownInteger` do pacote 3 para a contagem 1–16.
- Diagnósticos específicos usam enum próprio e conversão documentada aos
  códigos compartilhados já existentes; não foi necessário alterar o enum
  público do pacote 1. Nenhuma interface existente foi modificada.
- `encodeFormat2` valida os tipos fornecidos e emite somente opcode/nibbles;
  não há passagem, layout, PC/base ou codificação de formatos 3/4.
- Os PDFs não estavam disponíveis no workspace e não foram consultados.
  Não se afirma divergência ou concordância com PDFs não examinados; os
  bytes foram confrontados com o contrato e decoder disponíveis.

Verificação local:

- Formatação restrita aos seis novos arquivos de produção e aos cinco
  arquivos de testes/suporte; `clang-format --dry-run --Werror` passou.
- Builds limpos Release em `build/assembler-package2` e
  `build/assembler-package2-core`; CTest passou em **345 testes com Qt** e
  **342 sem Qt** (81 testes novos). Nenhum teste foi ignorado.
- Tabela esperada independente com todas as 38 entradas, opcodes, formatos,
  padrões e tamanhos; os dez opcodes de formato 2 fazem **656 round-trips**
  ao decoder cobrindo todos os registradores e contagens válidos. Bytes
  esperados manualmente e falhas com código/posição também são verificados.
- `git diff --check` passou. Comandos reproduzíveis em
  [testes](../../testing.md#montador-pacote-2).

Pendências para integração (roteiro de análise e correção no
[handoff ao agente do chefe](02-instruction-catalog-handoff.md)):

- Não foi encontrado acordo com o responsável do pacote 3 na documentação
  ou histórico inspecionados. A leitura de comentários do PR #24 pela API
  GitHub recebeu `Forbidden`, portanto não foi possível verificar eventual
  acordo remoto. Foi solicitado esclarecimento ao usuário; `SymbolTable.*`
  e suas oito diretivas permanecem inalterados até haver esse acordo.
  Substituir a lista local de mnemônicos pela consulta ao catálogo e testar
  essa integração continuam pendentes dessa coordenação.
- CI remota não executada/verificada para esta alteração; revisão de outro
  integrante não realizada. O checklist de CI permanece aberto. Não foi
  efetuado merge. A proposta de PR em rascunho, conforme o template, aguarda
  avaliação do chefe do projeto e acordo sobre a integração com o pacote 3.
