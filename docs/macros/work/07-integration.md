# Pacote 7 — integrador, MASMAPRG.ASM e aceitação

Responsável: um integrante. Estado: **planejado**. Depende dos pacotes
1 a 6. Pode preparar CLI e fixtures antes dos merges, mas integra somente
com a interface pública estabilizada. Ler [contratos](../contracts.md),
[uso do montador](../../assembler/usage.md) e `CONTRIBUTING.md`.

## Entrega e ownership

Criar um módulo principal/executável `SICXEMacroAssembler` que lê o arquivo
fonte informado, chama `macro::expand`, publica `MASMAPRG.ASM` no diretório
de trabalho e passa o texto expandido para `assembler::assemble`. Reutilizar
os writers existentes para `.bin`/`.lst`; nomes padrão vêm da fonte
**original**. Manter `SICXEAssembler` funcional e inalterado para montagem
direta. O integrador não cria um segundo parser/codificador de assembly.
Usar `SICXEMacroAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]`, com as
mesmas formas longas, `--`, `--help`/`-h` e códigos de retorno da CLI do
montador. O nome e o destino de `MASMAPRG.ASM` não são configuráveis.

Concentrar I/O e política de publicação aqui: proteger fonte e destinos,
substituir `MASMAPRG.ASM` existente com publicação segura, preservar a
versão anterior em erro de leitura/expansão/publicação e não publicar
binário/listagem se a montagem falhar. Se a expansão deu certo, manter a
**nova** versão de `MASMAPRG.ASM` mesmo quando a montagem ou a gravação de
`.bin/.lst` falhar.
Associar erros do montador à linha expandida e, quando
disponível, à origem/cadeia de chamadas. Documentar CLI e política de
saídas em `docs/macros/usage.md` no próprio PR. Ignorar no Git o
`MASMAPRG.ASM` gerado quando a ferramenta for executada na raiz do
repositório, sem ignorar outros arquivos-fonte `.asm`.

## Aceitação ponta a ponta

- Fonte com definição e chamada aninhadas; `MASMAPRG.ASM` contém apenas
  assembly do recorte SIC/XE e é aceito pelo montador **sem** modo especial.
- Comparar `.bin` e `.lst` com o resultado de uma fonte expandida
  manualmente; carregar somente bytes na VM e conferir estado/memória.
- Conferir nome e localização exatos de `MASMAPRG.ASM`, opção `--help`,
  substituição de arquivo existente, falhas de leitura/publicação e ausência
  de saídas parciais nos casos definidos em contratos. Em erro de montagem,
  verificar que a expansão nova permanece no arquivo.
- Rodar o fluxo sem Qt; também verificar configuração normal com Qt. CI
  executa os novos testes em ambas as configurações, sem remover cobertura
  atual do montador ou da VM.

## Não fazer

Não adicionar leitura de assembly à VM ou ao Qt, não chamar a CLI do
montador por subprocesso, não introduzir linker/loader/relocação, não
aceitar instruções vermelhas e não mudar o formato `.bin` existente.

## Aceite do PR

Atualizar `README.md`, [roadmap](../../roadmap.md),
[CONTRIBUTING](../../../CONTRIBUTING.md), [índice](../README.md) e
[contratos](../contracts.md) para o estado **implementado**, com comandos
reais. Todos os cenários têm testes automatizados; CMake/CTest, formatação,
CI e revisão cruzada passam antes do merge.
