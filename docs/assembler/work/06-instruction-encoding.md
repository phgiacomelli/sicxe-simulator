# Pacote 6 — Codificação das instruções SIC/XE

Estado: implementado e validado localmente; CI e revisão externa pendentes.

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

- [x] As 28 instruções de memória podem ser codificadas nos formatos
  permitidos, com testes dos modos e limites.
- [x] Bytes gerados são aceitos pelo decoder; falha não emite bytes.
- [x] `docs/assembler/encoding.md` descreve prioridades e limites.
- [ ] Build, formatação, CTest e CI verdes.

## Registro do PR

Entregues `InstructionEncoder.hpp/.cpp`, testes unitários e integração com
decoder/resolver/máquina. Interfaces, diagnósticos e exemplos estão em
[encoding.md](../encoding.md). Os formatos permitidos são 27 instruções de
memória em 3/4 e `RSUB` exclusivamente em 3, além das dez de formato 2 delegadas.

Decisões:

- `encodeInstruction` recebe linha e símbolos constantes e `BaseState` explícito
  (`optional<uint32_t>`); ausência de base é diferente de base ativa em zero.
- Reutiliza catálogo/operandos/formato 2 e resolução de valores dos Pacotes 2/3,
  sem alterar seus contratos nem os tipos dos Pacotes 4/5.
- Confere os campos redundantes do intermediário antes da emissão; não promove
  formato nem altera endereço/tamanho. Referências e estado de base não vazam
  entre chamadas.
- O chamador futuro do Pacote 7 mantém BASE/NOBASE. O encoder não recebe VM nem
  faz I/O. Fixtures de execução nos testes não introduzem uma passagem 2 pública.
- O limite de layout no último byte foi preservado; a restrição de PC pós-fetch
  da VM existente foi documentada e exercitada separadamente.
- Sem nova interpretação dos PDFs distribuídos fora do Git: foram seguidos os
  contratos do dialeto e a implementação atual da VM.

Ownership adicional: CMake e documentos de arquitetura/estado/testes. Nenhuma
alteração em núcleo, Qt ou implementação dos Pacotes 2–5 é necessária.
Builds Release em diretórios novos passaram em **388 testes com Qt** e
**385 sem Qt**, usando GCC 16.2.1, Qt 6.11.2 e GoogleTest 1.17.0. Nenhum teste
falhou. A fonte de GoogleTest existente foi reutilizada via
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`. O alvo `format` e a verificação global
`clang-format --dry-run --Werror` passaram em container Fedora 43 com
clang-format 21; `git diff --check` também passou. Os 15 novos testes incluem
540 combinações de modos/estratégias nas 27 instruções de memória,
round-trip dos 38 opcodes e dois programas com execução a partir de bytes.
CI será registrada após a publicação do PR.
Passagem 2 completa, arquivos de saída e CLI permanecem no Pacote 7; revisão
externa é necessária antes do merge.
