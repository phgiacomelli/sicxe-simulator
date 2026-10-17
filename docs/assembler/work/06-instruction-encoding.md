# Pacote 6 — Codificação das instruções SIC/XE

Estado: planejado. Responsável: a definir.

## Resultado esperado

Codificar as instruções de memória de formatos 3 e 4 aceitas pela VM,
com modos simples/imediatos/indiretos, indexação, PC/base e endereços
fixos. A saída é determinística e nunca muda o endereço/tamanho definidos
na passagem 1.

## Dependências e ownership

Consumir catálogo/operandos do pacote 2, símbolos do pacote 3,
endereços do pacote 4 e layout de dados do pacote 5. Arquivos sugeridos:
`src/assembler/InstructionEncoder.*`,
`tests/assembler/InstructionEncoderTest.cpp`,
`docs/assembler/encoding.md`. Mudanças no operando/intermediário exigem
revisão dos respectivos responsáveis.

## Interface a entregar

`encodeInstruction(locatedLine, symbols, baseState)` devolve 3/4 bytes
ou diagnóstico. O formato 2 delega ao módulo do pacote 2. O chamador
fornece o estado de `BASE`/`NOBASE` na ordem da fonte. O codificador
não lê registradores/memória da VM e não presume que `BASE` tenha
escrito o registrador B em tempo de execução.

## Comportamento obrigatório

- Para alvo simbólico em formato 3, preferir PC-relative assinado
  `[-2048,2047]`; se não couber, tentar base-relative unsigned
  `[0,4095]` quando houver `BASE`. Para número absoluto de até 12 bits,
  usar campo direto `b=p=0`. Nada de promoção automática para formato 4.
- `+` escolhe formato 4 com endereço **fixo** de 20 bits e `b=p=0`.
  Nenhum ajuste `M` ou relocação: o `.bin` só vale no `START` original.
- `#` fixa `n=0,i=1`; `@` fixa `n=1,i=0`; simples usa `n=i=1`;
  `,X` apenas no modo simples. Imediato numérico não negativo cabe no
  campo escolhido; negativo é erro conforme a VM atual.
- `RSUB` sem operando usa formato XE de 3 bytes e não admite `+`.
  Campo de endereço e bits extras ficam zerados.
- Se formato 3 não alcança o rótulo, emitir diagnóstico com sugestão de
  `+` quando aplicável; não mudar layout. Recusar endereço fora de 20
  bits e flags que o `Decoder` rejeitaria.

## Testes obrigatórios

- bytes esperados manualmente para `#`, `@`, simples, `,X`, PC positivo/
  negativo, base, formato 4 e `RSUB`;
- extremos PC `-2048/+2047`, base `0/4095`, campos 12/20 bits e casos
  logo além;
- `BASE` ativo/inativo e preferência PC quando ambos cabem;
- referência futura, endereço distante, número absoluto e imediato
  negativo;
- round-trip via `Decoder` e pequeno programa executado pela
  `Machine` **a partir de bytes**, sem passar assembly à VM.

## Fora do escopo

Não implementar parser, arquivo objeto, linker, relocação, literal,
formato SIC legado de fonte, formato 1 ou opcode vermelho.

## Critério de conclusão

- [ ] As 28 instruções de memória podem ser codificadas nos formatos
  permitidos, com testes dos modos e limites.
- [ ] Bytes gerados são aceitos pelo decoder; falha não emite bytes.
- [ ] `docs/assembler/encoding.md` descreve prioridades e limites.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Preencher ao implementar: decisões, divergências dos PDFs, interfaces
alteradas, testes e pendências.
