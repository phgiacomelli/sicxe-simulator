# Pacote 2 — captura de definições e tabela de macros

Responsável: um integrante. Estado: **planejado**. Depende do pacote 1;
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
