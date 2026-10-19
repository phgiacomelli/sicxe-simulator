# Pacote 2 — Pendências para análise e correção pelo agente do chefe

## Contexto e estado verificado

Branch: `feat/assembler-isa`, com destino a `main`.
Referência da implementação analisada: `26fe73b1814e6aca34c4a01b96c82de61c0a96f6`.
Este documento é posterior a essa referência; confira o HEAD e alterações
preexistentes antes de trabalhar. Preserve o checkout e o fluxo de
[CONTRIBUTING](../../../CONTRIBUTING.md).

O pacote 2 da segunda etapa entrega catálogo da ISA, operandos e formato 2.
A implementação e os testes locais estão disponíveis. A lista duplicada de
mnemônicos em `SymbolTable` permanece preservada; sua substituição depende da
coordenação exigida no [plano do pacote](02-instruction-catalog.md).
Este handoff não representa esse acordo nem uma aprovação de merge.

| Pendência | Natureza | Estado na entrega | Critério de encerramento |
| --- | --- | --- | --- |
| Acordo com o responsável pelo pacote 3 | Coordenação | Não confirmado | Acordo explícito registrado no PR, com escopo e responsável |
| Consulta de `SymbolTable` ao catálogo | Código e testes, após o acordo | Não implementada | Lista duplicada removida, oito diretivas separadas e testes aprovados |
| CI do PR | Validação remota | Não verificada | Checks do HEAD final aprovados, com evidência no PR |
| Revisão pelo chefe/integrante externo | Revisão | Pendente | Parecer registrado e apontamentos resolvidos |
| Abertura do draft pela API | Acesso operacional | Última tentativa recebeu `Forbidden` | Draft confirmado no GitHub, com URL real |

As pendências de CI/revisão não são falhas de código confirmadas. A integração
de nomes reservados é o trabalho de código conhecido ainda por realizar.

## Leitura obrigatória

Leia nesta ordem o [índice do montador](../README.md), os
[contratos](../contracts.md), [CONTRIBUTING](../../../CONTRIBUTING.md) e o
[plano do pacote 2](02-instruction-catalog.md). Depois consulte:

- [ISA implementada](../isa.md): novas interfaces, regras e diagnósticos;
- [fonte](../source.md): `SourceLine`, tokens e posições;
- [símbolos](../symbols.md) e [pacote 3](03-symbols.md): ownership e contratos;
- [decoder](../../decoder.md), [arquitetura](../../architecture.md),
  [testes](../../testing.md#montador-pacote-2) e [roadmap](../../roadmap.md).

Os PDFs citados pelo projeto não estavam disponíveis e não foram consultados.
Não atribua a eles conclusões baseadas somente no código ou nos documentos.

## 1. Confirmar e registrar o acordo com o pacote 3

O plano do pacote 2 exige: “Ao entregar o catálogo, combinar com seu responsável
a consulta de `SymbolTable` ao catálogo como fonte única desses nomes; manter
as 8 diretivas separadas, pois não são opcodes. Cobrir a integração com testes.”

Não foi localizado acordo na documentação/histórico inspecionados. A tentativa
de ler comentários do PR #24 pela API GitHub recebeu `Forbidden`; isso não
prova que um acordo remoto inexista.

Antes de editar os arquivos do pacote 3:

1. Verificar o PR e as decisões atuais da equipe.
2. Obter ou confirmar com o responsável do pacote 3 o acordo sobre a mudança.
3. Registrar a evidência no PR: quem concordou, qual escopo e quem executará.
4. Caso o chefe tenha autoridade para coordenar essa integração, registrar
   expressamente sua decisão; não inferir aprovação a partir deste documento.

Enquanto faltar o acordo, continuar a revisão do catálogo/operandos/encoder
e a validação independente. Não alterar interfaces compartilhadas por rotina.

## 2. Integrar nomes reservados ao catálogo, após o acordo

### Problema e pontos de alteração

[`src/assembler/SymbolTable.cpp`](../../../src/assembler/SymbolTable.cpp)
contém `ReservedNames`, uma lista ordenada com 46 palavras: 38 mnemônicos e
8 diretivas. A lista de instruções duplica o novo catálogo do montador e pode
divergir dele em futuras alterações.

Há quatro verificações atuais por `std::binary_search(ReservedNames...)`:

- `SymbolTable::isReservedName`;
- `SymbolTable::define`;
- `SymbolTable::reference`;
- `SymbolTable::resolve`.

Alterar somente `isReservedName` deixaria as demais verificações usando a
lista antiga. Revisar todas as ocorrências e centralizar a regra sem mudar
as assinaturas públicas.

[`src/assembler/OperandValue.cpp`](../../../src/assembler/OperandValue.cpp)
já usa `SymbolTable::isReservedName` dentro de `parseValue`. Verificar esse
caminho com testes; não é necessário reimplementar o parser de valores.

### Correção esperada

- Consultar o catálogo de
  [`InstructionCatalog.hpp`](../../../src/assembler/InstructionCatalog.hpp)
  para reconhecer os mnemônicos, usando `findInstruction` ou a enumeração
  imutável já exposta. Não criar outra lista local de 38 instruções.
- Manter uma lista própria somente para `START`, `END`, `BYTE`, `WORD`,
  `RESB`, `RESW`, `BASE`, `NOBASE`. Se usar `binary_search`, conservar a
  ordenação necessária.
- Validar primeiro o identificador conforme o contrato existente. Por exemplo,
  `+LDA` não é um nome válido de símbolo, embora o catálogo aceite `+` em
  consultas de instruções. Não transformar erros de nome em erros de reserva.
- Preservar comparação ASCII sem distinção de caixa, grafia original,
  posições, códigos de diagnóstico e ausência de mutação em falhas.
- Manter `InstructionCatalog` independente de `SymbolTable`/`OperandValue`.
  A consulta de nomes reservados não deve chamar parsing de operandos e
  retornar recursivamente à tabela de símbolos.
- Preservar `A`, `B`, `X` e outros identificadores válidos como nomes de
  símbolos quando não colidirem com mnemônicos/diretivas. Não reservar
  instruções fora do recorte apenas porque existem em outra ISA SIC/XE.

### Testes de aceitação da integração

Estender preferencialmente
[`SymbolTableTest.cpp`](../../../tests/assembler/SymbolTableTest.cpp) e
[`OperandValueTest.cpp`](../../../tests/assembler/OperandValueTest.cpp),
com coordenação do pacote 3. Cobrir:

- Todos os 38 mnemônicos do catálogo, também em caixa baixa/mista: recusados
  por `define`, `reference`, `resolve` e `parseValue` como nomes reservados.
- As oito diretivas recusadas como símbolos e ausentes do catálogo de opcodes.
- Diagnósticos com código e posição esperados, sem comparação de mensagem fixa.
- Identificadores inválidos ainda classificados como inválidos; nomes válidos
  não reservados, como `A`, `B`, `X`, `TARGET`, `ADD_LOCAL`, continuam aceitos.
- Uma falha de reserva não insere definição nem referência pendente.
- Regressão dos testes existentes de duplicidade, indefinição, tokens e faixas.

Concluir somente com o acordo registrado, os quatro caminhos consistentes e
os testes de integração aprovados.

## 3. Validar a alteração final e a CI

Evidência anterior à integração pendente: builds limpos Release passaram em
345 testes com Qt e 342 sem Qt; 81 testes novos, incluindo 656 round-trips de
formato 2. Formatação restrita e `git diff --check` passaram. Esses números são
uma referência da versão analisada, não uma exigência de contagem após novos
testes nem prova de validação do futuro fix.

Na raiz do repositório, com as ferramentas no PATH:

```bash
cmake -S . -B build/package2-review -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/package2-review --parallel 4
ctest --test-dir build/package2-review --output-on-failure -j 4

cmake -S . -B build/package2-review-core -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSICXE_BUILD_QT=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build/package2-review-core --parallel 4
ctest --test-dir build/package2-review-core --output-on-failure -j 4

git diff --check
```

Neste ambiente, as ferramentas locais podem ser ativadas com
`source /workspace/.sicxe-env/activate.sh`. Em outra máquina, instalar os
pré-requisitos conforme o README; não presumir que esse arquivo exista.
Usar diretórios novos de build ou verificar previamente seu conteúdo.
Formatar somente os arquivos C++ alterados com `clang-format -i` e conferir
os mesmos arquivos com `clang-format --dry-run --Werror`.

A CI existente está em
[`.github/workflows/ci.yml`](../../../.github/workflows/ci.yml), no container
Fedora 43, e já verifica formatação, build e CTest com/sem Qt. Ela roda em
`pull_request` e em push para `main`; publicar a branch de feature, isoladamente,
não comprova sua execução. Verificar o run do HEAD final e registrar a URL e
resultado reais. Não marcar CI verde com base somente nos testes locais.

## 4. Revisar e atualizar os registros

O chefe ou integrante fora do pacote deve revisar catálogo, operandos,
formato 2 e a integração acordada. Avaliar especialmente a preservação dos
tokens/posições e a compatibilidade com os consumidores dos pacotes 4/6.

Uma decisão a conferir está documentada em [ISA](../isa.md#diagnósticos):
`InstructionDiagnostic` usa códigos específicos e é convertido para códigos
mais gerais de `AssemblyDiagnosticCode`. Essa decisão preservou o enum
compartilhado existente; avaliar sua adequação aos consumidores. Trata-se de
ponto de revisão, não de defeito confirmado. Mudanças de contrato exigem a
coordenação e a documentação previstas no projeto.

Após a correção/validação efetivas, atualizar os documentos que registram a
pendência: [ISA](../isa.md), [contratos](../contracts.md),
[Registro do PR](02-instruction-catalog.md#registro-do-pr),
[índice](../README.md), [testes](../../testing.md#montador-pacote-2),
[roadmap](../../roadmap.md) e [README principal](../../../README.md), conforme
necessário. Se a integração modificar a responsabilidade de símbolos,
atualizar também [símbolos](../symbols.md). Não marcar itens concluídos sem
evidência nem apagar a distinção entre validação local, CI e revisão externa.

## 5. Acesso ao GitHub e estado do draft

A branch foi publicada e seu HEAD remoto foi confirmado. A tentativa de criar
o PR com `gh pr create --draft` foi bloqueada por
`POST https://api.github.com/graphql: Forbidden`. Havia `GH_TOKEN` injetado;
não foi solicitado nem copiado token. A autenticação Git existente funcionou
para leitura e push, o que não comprova acesso à API.

A inclusão de `api.github.com` foi salva no rascunho da configuração do
ambiente; salvar esse rascunho pela ferramenta não aplicou a mudança ao runtime.
Para prosseguir pela API, revisar/salvar/publicar a configuração nas opções do
ambiente e testar novamente a operação. Se o acesso continuar negado, diagnosticar
rede/autorização antes de pedir novas credenciais. Não expor valores de segredos.

Também é possível abrir o formulário pelo GitHub:
<https://github.com/phgiacomelli/sicxe-simulator/pull/new/feat/assembler-isa>.
Confirmar se já existe PR antes de criar outro e escolher draft enquanto as
pendências estiverem abertas. Não há número de PR confirmado nesta entrega.
Não presumir que esse estado permaneça igual quando o chefe iniciar a análise.

## Limite do fix e checklist final

O trabalho continua restrito ao pacote 2 e à integração explicitamente acordada
com o pacote 3. Não implementar passagens 1/2 completas, resolução nova de
símbolos, alocação, PC/base, formatos 3/4, CLI ou novas instruções. Não alterar
Qt, a VM ou dependências para acomodar o montador. Não fazer merge.

- [ ] Acordo do pacote 3 registrado e verificável.
- [ ] Catálogo como fonte única dos mnemônicos em todos os caminhos de reserva.
- [ ] Oito diretivas mantidas separadas dos opcodes.
- [ ] Testes novos e regressões aprovados com e sem Qt.
- [ ] Formatação restrita e revisão do diff concluídas.
- [ ] CI aprovada no HEAD final, com evidência.
- [ ] Revisão externa e apontamentos registrados/resolvidos.
- [ ] Registros/documentos atualizados com o estado real.
- [ ] Draft existente confirmado ou criado, sem duplicação e sem merge.
