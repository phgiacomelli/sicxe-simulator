# Como contribuir

Faça mudanças pequenas, com um objetivo claro. Antes de começar, confira o
código e os testes da parte que pretende alterar. Combine com o grupo as
mudanças que afetarem interfaces usadas por outros integrantes.

## Preparar uma branch

Crie sua branch a partir da `main` atualizada:

```bash
git switch main
git pull --ff-only
git switch -c feat/nome-da-mudanca
```

Use um nome que descreva a tarefa. Os prefixos `feat/`, `fix/`, `test/`,
`refactor/` e `chore/` ajudam a identificar o tipo de mudança. Evite
trabalhar diretamente na `main`.

## Alterar o código

- Mantenha o núcleo da máquina, o montador e o processador de macros
  independentes de Qt.
- A máquina virtual executa bytes. O montador transforma assembly em bytes.
  O processador de macros prepara o código-fonte antes da montagem.
- Siga o estilo dos arquivos existentes e as regras de `.clang-format`.
- Use `#pragma once` nos headers e inclua o próprio header primeiro nos
  arquivos `.cpp`.
- Nos testes, inclua headers a partir de `src`, como `core/Memory.hpp`,
  sem usar caminhos com `../src`.
- Acrescente ou ajuste testes quando mudar o comportamento. Considere os
  casos de erro e os limites da mudança.

## Verificar a mudança

Compile e execute os testes:

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Para trabalhar sem Qt, configure com `-DSICXE_BUILD_QT=OFF`, como mostrado
no [README](README.md#compilar-e-executar).

Se tiver `clang-format` instalado, configure o CMake e use o alvo de
formatação antes de revisar o resultado:

```bash
cmake --build build --target format
```

Confira o `git diff` e evite incluir alterações sem relação com a tarefa.
Atualize o README se mudar a forma de compilar ou usar o projeto.

## Registrar e integrar

Escreva commits curtos que expliquem a mudança, por exemplo:

```text
feat: add parameter substitution
fix: reject invalid register
test: cover empty input
```

Antes de integrar à `main`, peça a revisão de outro integrante e aguarde
as verificações automáticas passarem. Explique o que mudou, como foi
testado e se há alguma limitação conhecida.
