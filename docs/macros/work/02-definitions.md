# Pacote 2 — captura de definições e tabela de macros

Responsável: um integrante. Estado: **implementado, revisado tecnicamente e
validado**, com CI verde no [PR #32](https://github.com/phgiacomelli/sicxe-simulator/pull/32).
Depende do pacote 1;
pode evoluir em paralelo ao 3. Ler [contratos](../contracts.md).

## Entrega e ownership

Implementar o módulo interno que recebe a linha `MCDEFN`, lê o protótipo na
próxima linha ativa e captura o esqueleto até o `MCEND` correspondente.
Usar **contador de nível de definição**, não busca pelo primeiro `MCEND`.
Guardar corpos aninhados como parte da definição externa; não instalar a
definição interna na tabela antes de sua linha ser produzida por uma expansão.
O motor do pacote 6 controla quando chamar esse módulo.

A tabela indexa nomes sem distinção de caixa. Uma nova definição do mesmo
nome substitui a versão ativa para chamadas subsequentes, mantendo a versão
capturada por uma expansão já iniciada. Validar identificadores e nomes
reservados pela fonte de verdade do montador (`SymbolTable::isReservedName`
ou função equivalente), mais `MCDEFN`/`MCEND`; não copiar a lista de opcodes.
Arquivos esperados: `MacroDefinition.*`, `DefinitionCapture.*`,
`MacroTable.*` e testes correspondentes em `tests/macros/` (nomes podem mudar
sem mudar o ownership).

## Casos obrigatórios

- Definição simples, aninhamento com dois ou mais níveis e corpos vazios.
- `MCEND` isolado, falta de `MCEND`, falta de protótipo, protótipo inválido,
  `MCDEFN` com campos indevidos e nome reservado.
- Duas definições do mesmo nome: chamadas anteriores e posteriores usam
  versões diferentes; capitalização não cria nomes distintos.
- Definição interna fica invisível até a expansão da externa; antes disso,
  chamada ao seu nome passa sem expansão e é rejeitada pelo montador na
  integração.
- Comentários e linhas vazias dentro do esqueleto não alteram a contagem.

## Não fazer

Não expandir chamadas nem substituir parâmetros; não produzir arquivos.
Não recusar redefinição só por ter tamanho diferente: os slides explicitam
que a nova definição pode ser maior ou menor.

## Aceite do PR

Testes exercitam a interface do módulo de captura/tabela sem inspecionar
estrutura privada. CMake/CTest, formatação e documentação passam. Mudanças
na forma do protótipo ou da tabela exigem alinhamento com os pacotes 3, 4 e 6.

## Registro da implementação

### Arquivos e decisões

- `src/macros/MacroDefinition.hpp/.cpp`: valor próprio com consultas const,
  protótipo bruto, corpo e delimitadores externos com posições físicas.
- `src/macros/DefinitionCapture.hpp/.cpp`: captura por contador de níveis,
  próximo índice para o motor e falha tipada sem definição parcial.
- `src/macros/MacroTable.hpp/.cpp`: instalação explícita e consultas ASCII
  sem distinção de caixa, com handles imutáveis que sobrevivem à redefinição.
- `tests/macros/DefinitionCaptureTest.cpp` e `MacroTableTest.cpp`, registrados
  no `MacroTests`; fontes registradas no `sicxe_macros`.
- Dependência privada em `sicxe_assembler` exclusivamente para reutilizar
  validação/normalização de nomes. Evitou-se extrair ou duplicar seu catálogo.
  Nenhum arquivo do montador, VM ou interface foi alterado.

A forma C++ e as regras de posição/diagnóstico ficam no
[contrato comum](../contracts.md#interfaces-internas-entregues-no-pacote-2).
Não há parsing de formais, substituição, serial, chamadas, motor ou I/O.
As definições internas permanecem brutas no esqueleto; seus protótipos não
são instalados/validados antecipadamente. Os campos dos delimitadores são
verificados em todos os níveis para garantir a estrutura capturada.

### Testes e validação local

Os 21 testes novos cobrem captura (17) e tabela (4), incluindo três níveis,
comentários estruturais, posições, falhas tipadas, redefinições de tamanhos
diferentes e duração das versões anteriores. Não há testes de expansão ou
integração que antecipem os pacotes 6/7.

Builds novos passaram em **446 testes sem Qt (Debug)** e **449 com Qt
(Release)**, sem falhas ou casos ignorados. O alvo `format`, a verificação
`clang-format --dry-run --Werror` em todo o C++, `git diff --check` e a
compilação isolada dos novos fontes/headers com avisos como erros passaram.
Ninja não estava instalado; usou-se Unix Makefiles com GoogleTest já local,
sem instalação de dependências. Comandos reproduzíveis em
[testes](../../testing.md#macros-pacote-2).

### Handoff e revisão

- Pacote 3: consumir a linha completa de `prototype()` e suas posições;
  validar formais, aspas, repetições e sintaxe de argumentos separadamente.
- Pacote 4: consumir `body()` com níveis internos completos e campos brutos;
  implementar seu próprio modelo de referências com escopo.
- Pacote 6: chamar captura no ponto de leitura, avançar por `nextLine`,
  instalar explicitamente com `define()` e reter o handle de `find()`.
  Aplicar a política de comentários estruturais e adicionar contexto aos erros.

As interfaces são aditivas; nenhum campo do pacote 1 foi alterado. A revisão
técnica do PR #32 verificou padrões, requisitos e os dados necessários aos
consumidores, sem bloqueadores de código ou desvios de escopo. Builds novos
em Release repetiram **446 testes sem Qt e 449 com Qt**, todos aprovados;
formatação, avisos como erros e o job remoto `Format, build and test` passaram.
Não foi registrada aprovação nominal dos responsáveis pelos pacotes 3, 4 e 6;
essa confirmação permanece como acompanhamento do handoff, sem ser atribuída
ao parecer técnico.

- [x] Captura, tabela e preservação de versões implementadas.
- [x] Testes pelas interfaces, sem depender do motor ou de estrutura privada.
- [x] Contratos, índice e roadmap atualizados.
- [x] Formatação, build e CTest locais com e sem Qt aprovados.
- [x] Revisão técnica do handoff aos pacotes 3, 4 e 6 registrada no PR #32.
- [x] CI remota verde e parecer técnico de padrões/requisitos registrado.
- [ ] Confirmação nominal das interfaces pelos responsáveis dos pacotes 3, 4 e 6.
