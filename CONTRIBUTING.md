# Como contribuir

Este projeto usa branches curtas e pull requests para manter a `main` sempre
compilável. Cada mudança deve preservar os contratos entre os módulos e incluir
sua própria verificação.

## Antes de começar

1. Atualize sua `main` local.
2. Leia [a arquitetura da VM](docs/architecture.md) e a documentação do módulo
   que será alterado.
3. Confirme no [roadmap](docs/roadmap.md) a ordem atual do projeto.
4. Leia o seu [pacote de trabalho](docs/work/README.md) e identifique ownership,
   dependências e critérios de conclusão.

Mudanças em interfaces compartilhadas devem ser combinadas com os integrantes
afetados antes da implementação.

## Fronteira obrigatória da VM

A VM recebe somente bytes de código de máquina já codificados. Nenhuma mudança
pode atribuir a ela responsabilidades de montador: interpretar assembly,
mnemônicos, diretivas, labels ou tabelas de símbolos; gerar ou interpretar
registros de formato objeto; fazer linking ou aplicar relocação.

Texto hexadecimal pode ser aceito somente como representação dos bytes de
entrada. Da mesma forma, um arquivo binário bruto é apenas uma sequência de bytes,
não um arquivo objeto. A definição completa dessa fronteira está na
[arquitetura da VM](docs/architecture.md#fronteira-entre-vm-e-montador).

## Branches

Crie a branch a partir da `main` atualizada:

```bash
git switch main
git pull --ff-only
git switch -c feat/nome-curto
```

Prefixos recomendados:

| Prefixo | Uso |
| --- | --- |
| `feat/` | Nova funcionalidade |
| `fix/` | Correção de comportamento |
| `test/` | Testes sem mudança funcional |
| `docs/` | Documentação |
| `refactor/` | Reestruturação sem mudança de comportamento |
| `chore/` | Infraestrutura e manutenção |

Não desenvolva diretamente na `main`.

## Commits

Use mensagens curtas no padrão Conventional Commits adotado pelo histórico:

```text
feat: implement address resolver
fix: reject invalid indexed addressing
test: add register file tests
docs: document resolved operands
chore: add continuous integration
```

Cada commit deve representar uma alteração coerente. Separe implementação,
testes e documentação quando isso tornar a revisão mais clara.

## Ownership e interfaces

- Altere prioritariamente os arquivos pertencentes ao seu pacote.
- Registre no pull request qualquer alteração necessária fora desse ownership.
- Preserve a direção de dependências descrita em `docs/architecture.md`.
- Mantenha o núcleo independente de Qt.
- Regras de endereçamento pertencem ao `AddressResolver`; regras das operações
  pertencem ao `Executor`; somente a `Machine` coordena o ciclo completo.
- Evite duplicar uma decisão arquitetural em mais de um documento. Atualize a
  fonte principal e use links nos demais locais.

## Convenções C++

- Headers do projeto usam `#pragma once`.
- Um arquivo `.cpp` inclui primeiro o próprio header, depois outros headers do
  projeto e por fim a biblioteca padrão ou dependências externas.
- Testes incluem headers a partir da raiz configurada em `src`, por exemplo
  `#include "core/Memory.hpp"`; não use caminhos relativos com `../src`.
- Constantes arquiteturais compartilhadas devem ter um único nome e uma única
  definição, como `WordMask` em `Architecture.hpp`.
- Falhas esperadas atravessam as interfaces tipadas do núcleo; exceções de
  infraestrutura devem ser convertidas no módulo que conhece seu significado.
- Operações do executor calculam efeitos e permanecem sem mutação. A ordem de
  validação e commit pertence exclusivamente à `Machine`, conforme a
  [arquitetura](docs/architecture.md).

## Verificação local

Configure e compile:

```bash
cmake -S . -B build
cmake --build build
```

Execute os testes:

```bash
ctest --test-dir build --output-on-failure
```

Formate o código:

```bash
cmake --build build --target format
```

Antes de abrir o pull request, confirme que o formatador não deixou diferenças
inesperadas e que todos os testes passam.

## Documentação

Atualize a documentação quando uma mudança alterar:

- responsabilidade ou interface de um módulo;
- comportamento observável;
- formato de entrada ou saída;
- código de falha;
- estado ou ordem do roadmap;
- instruções de build, execução ou testes.

Decisões relevantes devem registrar o motivo e as alternativas consideradas,
não apenas o resultado escolhido.

## Critério de conclusão

Um pull request está pronto para integração quando:

- o escopo descrito foi concluído;
- casos normais, limites e falhas relevantes possuem testes;
- build e CTest passam em uma configuração limpa;
- `clang-format` passa;
- documentação afetada foi atualizada;
- nenhuma responsabilidade de montador foi introduzida na VM;
- pendências conhecidas estão declaradas;
- não existem alterações não relacionadas ao objetivo;
- pelo menos um integrante fora do pacote revisou a mudança.

O merge só deve ocorrer depois que a integração contínua estiver verde.
