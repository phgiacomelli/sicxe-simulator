# Montagem de programas: passagem 2, arquivos e CLI

Estado: pacote 7 implementado e validado localmente. Revisão cruzada e CI
remota ainda devem ocorrer no PR. O montador é uma ferramenta separada da
VM: recebe assembly e produz bytes; a VM continua recebendo somente bytes.

## Compilar e montar

Da raiz do repositório, com CMake e compilador C++20:

```bash
cmake -S . -B build -DSICXE_BUILD_QT=OFF
cmake --build build --parallel 4
./build/SICXEAssembler tests/assembler/fixtures/sum-and-table.asm \
  -o build/sum-and-table.bin -l build/sum-and-table.lst
```

O executável também é criado no build normal com Qt. Qt não é dependência
do montador. Em geradores multiconfiguração, o executável fica na pasta da
configuração escolhida, por exemplo `build/Debug/SICXEAssembler.exe`.
Para compilar somente a ferramenta, sem buscar GoogleTest, configure também
`-DBUILD_TESTING=OFF` e compile o alvo `SICXEAssembler`.

```text
SICXEAssembler SOURCE [-o OUTPUT.bin] [-l OUTPUT.lst]
```

- `SOURCE` é o arquivo-fonte; caminhos com espaços devem ser citados no shell.
- `-o`/`--output` escolhe o binário; `-l`/`--listing` escolhe a listagem.
- Sem essas opções, a extensão da fonte é substituída por `.bin` e `.lst`,
  no mesmo diretório. Os caminhos de saída são relativos ao diretório atual.
- `--` encerra as opções e permite um nome de fonte iniciado por `-`.
- `--help` ou `-h`, sozinho, exibe o uso e retorna sucesso.
- Retornos: `0` para sucesso/ajuda, `1` para erro de leitura, montagem ou
  gravação, `2` para argumentos inválidos.

O stdout informa os caminhos gerados, quantidade de bytes, endereço de carga
e PC inicial em hexadecimal. Erros da fonte saem no stderr como
`arquivo:linha:coluna: error: mensagem`, com posições iniciadas em 1.
Erros de I/O indicam o caminho afetado. Não há execução automática após montar.

### Política dos arquivos de saída

Os diretórios de saída precisam existir, e os dois destinos devem ser distintos
e **ainda não existir**. Arquivos, diretórios e links simbólicos já existentes
não são substituídos; a própria fonte também é protegida por essa regra.
Não há `--force`. Para repetir, escolha novos nomes ou remova conscientemente
as saídas anteriores. Nenhum `.bin` gerado precisa ser versionado.

Somente após uma montagem bem-sucedida os dois conteúdos são gravados e
fechados em diretórios temporários exclusivos, nos diretórios dos respectivos
destinos. A publicação usa hard links para não sobrescrever arquivos nem em
uma criação concorrente. Portanto, o sistema de arquivos dos destinos precisa
suportar **hard links** (por exemplo, ext4 ou NTFS; FAT/exFAT não servem).
Os destinos podem estar em sistemas de arquivos diferentes.

Falhas anteriores à publicação não criam saídas finais. Se a publicação da
listagem falhar após a do binário, o binário recém-criado é removido. Se essa
remoção também falhar, o erro `RollbackFailed` informa o caminho que precisa
ser removido manualmente; a CLI retorna erro. Isso não é uma transação durável
entre dois arquivos: desligamento abrupto ou interferência externa durante a
publicação não têm garantia de recuperação automática.

## Programa de demonstração e teste na VM

A fonte [sum-and-table.asm](../../tests/assembler/fixtures/sum-and-table.asm)
exercita referências futuras, formatos 2/3/4, saltos relativos ao PC,
subrotina, `BASE`/`NOBASE`, `WORD`, `BYTE` e reservas. Ela:

1. soma os inteiros de 1 a 5 e grava `15` em `SUM`;
2. chama `DOUBLE`, que multiplica o valor por 2;
3. carrega B explicitamente e usa acesso base-relative para ler `TABLE = 7`;
4. grava `37` em `RESULT` e permanece em `DONE J DONE`.

`BASE` apenas informa a montagem: é `+LDB #TABLE` que carrega B na execução.
A reserva de 4096 bytes força a distância usada no teste da base. A imagem
tem **4160 bytes**, incluindo zeros nas reservas, e não contém cabeçalho.

Para testar graficamente, use um build com Qt e abra `SICXESimulator`.
Em **Carregar Programa**, selecione o `.bin` bruto e informe endereço de
carga **`100`** e PC inicial **`100`** (campos em hexadecimal). Execute
**28 passos**, a partir da carga/reset, ou pause a execução contínua em `DONE`.

| Estado esperado após 28 passos | Valor |
| --- | --- |
| A | 37 decimal (`0x25`) |
| X / S | 5 / 5 |
| B | `0x1138` |
| L / PC | `0x116` / `0x126` |
| Condition Code | Equal |
| Palavra em `SUM`, endereço `0x129` | 15 (`00 00 0F`) |
| Palavra em `RESULT`, endereço `0x12C` | 37 (`00 00 25`) |
| Palavra em `TABLE`, endereço `0x1138` | 7 |
| Palavra em `PTR`, endereço `0x113B` | `0x129` |
| Bytes em `CHAR`, endereço `0x113E` | `4F 4B` (`OK`) |

Não existe `HALT` sintético nem parada automática no fim da imagem. O laço
final é intencional; use pausa. O `.bin` precisa ser carregado no `START`
informado pela CLI: não há relocação, loader de objeto ou integração do
montador ao editor/GUI. Números na fonte usam decimal ou prefixo `0x`;
`START 100` significaria 100 decimal, não `0x100`.

## APIs e responsabilidade de cada módulo

Todas as APIs abaixo vivem em `assembler`, sob `src/assembler/`.

- `assemble(std::string_view)` em `Assembler.hpp` compõe `SourceParser`,
  `PassOne` e `PassTwo`. Retorna `AssemblyResult`, não faz I/O e não recebe VM.
- `PassTwo::run(const PassOneResult&)` recusa diagnósticos da primeira
  passagem, percorre as linhas na ordem original e mantém `BaseState` local.
  Resolve `BASE` com a tabela final e limpa a base em `NOBASE`. Delega
  instruções a `encodeInstruction` e dados/reservas a `encodeData`.
- O tamanho codificado precisa coincidir com `LocatedLine::sizeBytes`;
  endereços e tamanhos não são recalculados nem formatos promovidos.
- `RawImageWriter::build(start, length, listing)` valida imagem não vazia,
  faixa de 20 bits e layout contíguo, sem lacunas, sobreposições ou truncagem.
  Emite o vetor de bytes com zeros nas reservas; não abre arquivos.
- `ListingWriter::render(success)` produz texto determinístico, com endereço
  de carga, entrada, tamanho, linhas físicas, texto original/comentários,
  bytes ou `RESERVE n`, e símbolos na ordem de definição. Endereços/bytes
  são hexadecimais; tamanhos e números de linha são decimais.
- `ListingWriter::diagnostics(failure, sourceName)` formata os erros da fonte.
  Em falha não é publicada uma `.lst` de sucesso parcial.
- `runAssemblerCli(args, out, errors)` trata argumentos e leitura da fonte;
  `writeProgramFiles(success, binPath, lstPath)` concentra o I/O das saídas.
  Retorna diagnóstico opcional tipado de arquivo, separado dos erros da fonte.

`sicxe_assembler` contém o pipeline puro e os writers em memória;
`sicxe_assembler_cli` contém CLI e publicação de arquivos;
`SICXEAssembler` contém apenas a adaptação de `argv`. Essas bibliotecas não
dependem de Qt nem de `SICXECore`; somente os testes de aceitação usam a VM.
Não há alterações em `core`, `application` ou `ui` para receber assembly.

O sucesso contém `loadAddress`, `initialPc`, `bytes`, `listing` e `symbols`.
A falha contém **somente diagnósticos**, mesmo quando o erro é encontrado
na última linha codificada. O pacote 7 acrescenta os códigos de fonte
`EmptyProgram` e `LayoutMismatch`; erros dos módulos anteriores mantêm sua
conversão existente para `AssemblyDiagnostic`.

Um programa só de reservas não vazias é válido; um programa de extensão
total zero recebe `EmptyProgram`. `BASE 0` é uma base ativa, não uma sentinela.
Chamadas sucessivas não compartilham estado de base ou símbolos.

## Verificação

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure \
  -R 'AssemblerTest|AssemblerCli|AssemblerAcceptanceTest'
```

Os 26 testes do pacote cobrem API pública, layout, listagem, erros, arquivos
reais e o executável. Os bytes da demonstração são esperados independentes,
calculados manualmente; a aceitação compara a memória completa e o estado
após execução real em `Machine`. Outro teste monta as 38 instruções pelo
pipeline completo e verifica os opcodes com o decoder.
Consulte [testes](../testing.md#montador-pacote-7) e o
[registro do pacote](work/07-pass-two-output.md).

As exclusões do [dialeto](contracts.md) permanecem: nenhuma expressão,
instrução vermelha, macro, literal, relocação, registro `H/T/M/E`, linker,
montador de uma passagem ou *load-and-go* foi acrescentado.
