# Pacote 2 — Catálogo da ISA, operandos e formato 2

Estado: planejado. Responsável: a definir.

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

- [ ] Catálogo coincide com o recorte da VM, sem lista de opcodes vermelhos.
- [ ] Formato 2 é utilizável diretamente pela passagem 2 e testado.
- [ ] `docs/assembler/isa.md` registra operadores e formatos aceitos.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Preencher ao implementar: decisões, divergências dos PDFs, interfaces alteradas,
testes e pendências.
