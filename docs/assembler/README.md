# Segunda etapa — montador SIC/XE de duas passagens

Estado: pacotes 1–7 implementados e integrados. O pacote final tem testes
locais, revisão técnica e CI verde registrados no
[PR #30](https://github.com/phgiacomelli/sicxe-simulator/pull/30).

As duas referências, distribuídas à equipe fora do Git, têm papéis diferentes:
`PRIMEIRO_TRABALHO_SIC_XE.pdf` define a **máquina SIC/XE deste projeto**
(registradores, modos, formatos e instruções não vermelhas);
`Montador.pdf` explica o **algoritmo de duas passagens** (contador de
localização, símbolos, intermediário, tradução e listagem). Os exemplos
`COPY`, `SPACE` e `CONST` dos slides pertencem a outra máquina didática;
não são opcodes ou diretivas da nossa SIC/XE.

## Resultado e fronteira

O objetivo da etapa é receber um arquivo assembly para o **recorte SIC/XE
já implementado na VM** e produzir bytes de código de máquina e uma
listagem. A passagem 1 atribui endereços e resolve a tabela de símbolos;
a passagem 2 codifica as
instruções e os dados usando esses endereços. Um executável de linha de
comando grava `.bin` bruto, que a interface atual da VM já carrega no
endereço informado. O montador não executa instruções, e a VM não interpreta
assembly.

Atualmente, `sicxe_assembler` oferece o parser, os tipos mínimos descritos em
[fonte](source.md), a tabela de símbolos e valores de operandos em
[símbolos](symbols.md), o catálogo, operandos de instrução e formato 2
em [ISA](isa.md) e a primeira passagem com contador de localização em
[passagem 1](pass-one.md), além dos [dados e reservas](data.md) integrados ao
contador. A [codificação das instruções](encoding.md) oferece formatos 2/3/4;
`assemble()`, passagem 2, `.bin`, `.lst` e CLI estão implementados no pacote 7,
conforme o [guia de uso e integração](usage.md).
A interface da VM continua recebendo bytes.

```text
fonte SIC/XE → parser → passagem 1 → linhas localizadas + símbolos
                                    → passagem 2 → bytes + listagem
                                                   ↓
                                             arquivo .bin
                                                   ↓
                                        entrada existente da VM
```

O código novo fica em `src/assembler/`, com testes em `tests/assembler/`.
Não introduzir parsing de assembly, rótulos ou diretivas em `src/core/`,
`src/application/` ou `src/ui/`. Não criar linker, loader de objeto,
relocação na carga, editor assembly, macros, montador de um passo ou
*load-and-go*.

## Recorte funcional

- Exatamente as **38 instruções** não vermelhas do primeiro trabalho, já
  reconhecidas pela VM: formato 2 e formatos 3/4. Nenhum opcode vermelho,
  formato 1, operando com registrador `F` ou `HALT` sintético.
- Rótulos locais com referências anteriores e futuras, tabela de símbolos,
  detecção de duplicados/indefinidos e contador de localização em **bytes**.
- Modos imediato, indireto, simples, indexado, relativo ao PC, relativo à
  base e formato 4 explícito, conforme os bits aceitos pelo `Decoder`.
- Apenas as convenções mínimas de fonte/dados definidas em
  [contratos](contracts.md): `START`, `END`, `BYTE`, `WORD`, `RESB`, `RESW`,
  `BASE`, `NOBASE`, `#`, `@`, `,X` e `+`. Elas tornam possível expressar a
  arquitetura do primeiro trabalho; o PDF **não prescreve** essa gramática.
- Saída fixa: bytes `.bin`, endereço de carga/PC inicial e listagem de linha,
  endereço e bytes/reserva. A imagem é válida no endereço escolhido por
  `START`; não é relocável.

## Fora do recorte

Não implementar recursos genéricos citados nos slides mas não definidos para
esta SIC/XE: `EQU`, `ORG`, `LTORG`, literais `=...`, expressões aritméticas,
símbolos absolutos/relativos ou externos, múltiplos contadores/seções,
símbolos redefiníveis, atributos `L()`/`A()`, ponto flutuante, registros
`H/T/M/E`, registros de modificação, tabela de referências cruzadas ou
formato objeto para um loader. Esses termos **não devem ganhar uma
implementação parcial** nesta etapa; a fonte deve receber erro claro se
usar sintaxe não suportada. Os slides servem como explicação do método,
não como autorização para estender o dialeto.

A VM aceita o modo SIC legado (`n=i=0`) em bytes já codificados. Este
montador produz instruções SIC/XE nos formatos 2/3/4; não inventaremos uma
sintaxe de fonte para solicitar o formato SIC legado.

## Sete pacotes, sete responsáveis

Cada integrante escolhe um pacote e entrega **implementação, testes,
documentação afetada e seu próprio PR**. Os pacotes são comparáveis em
esforço: todos possuem regras de sucesso, limites e falhas, não apenas uma
tarefa de documentação ou de integração alheia.

| Pacote | Entrega principal | Dependências de merge | Estado |
| --- | --- | --- | --- |
| [1 — Fonte e contratos](work/01-source-parser.md) | Parser, linhas e diagnósticos | Nenhuma; primeiro PR | Integrado |
| [2 — ISA e operandos](work/02-instruction-catalog.md) | Catálogo de 38 instruções e formato 2 | 1 | Implementado; integrado aos símbolos |
| [3 — Símbolos](work/03-symbols.md) | Definições, referências e valores numéricos/simbólicos | 1 | Implementado |
| [4 — Passagem 1](work/04-pass-one.md) | Contador, rótulos, `START`/`END` e intermediário | 1, 2, 3 | Implementado |
| [5 — Dados e reservas](work/05-data-reservations.md) | `BYTE`/`WORD`/`RESB`/`RESW` e integração ao layout | 1, 3, 4 | Implementado; integrado à passagem 1 |
| [6 — Codificação XE](work/06-instruction-encoding.md) | Formatos 3/4 e modos de endereçamento | 1, 2, 3, 4, 5 | Implementado; integrado aos módulos anteriores |
| [7 — Passagem 2 e saídas](work/07-pass-two-output.md) | Pipeline final, `.bin`, listagem, CLI e aceitação | 1–6 | Implementado; pipeline integrado e validado |

As dependências são de **merge**, não impedem desenvolvimento paralelo.
O pacote 1 entrega os tipos compartilhados mínimos junto do parser; não
haverá um PR extra só de contratos. Pacotes posteriores podem preparar
testes e código em suas branches. Mudança de interface exige acordo com os
consumidores, atualização de [contratos](contracts.md) e revisão cruzada.
O pacote 7 pode preparar CLI e casos de aceitação cedo, mas integra o
pipeline somente após as dependências. Manter branches curtas e `main`
sempre compilável.

## Fluxo de cada integrante

1. Atualizar `main`; ler este índice, [contratos](contracts.md),
   [CONTRIBUTING](../../CONTRIBUTING.md) e o próprio pacote.
2. Criar branch curta e implementar apenas o ownership do pacote. Se precisar
   alterar outro módulo, combinar com seu responsável e registrar no PR.
3. Testar pela interface pública, incluindo casos normais, limites e falhas.
   Atualizar a documentação do módulo e a seção “Registro do PR” do pacote.
4. Rodar formatação, build e CTest; abrir PR com escopo, testes, decisões e
   pendências. Um integrante de outro pacote revisa; merge só com CI verde.
5. Atualizar o [roadmap](../roadmap.md) e o estado deste índice no PR que
   concluir um pacote.

Os programas de aceitação terão bytes esperados calculados manualmente.
O teste final monta a fonte, carrega **somente os bytes** na VM e compara
o estado esperado; nunca passa assembly à `Machine`.
