# Pacote 6 — Interface gráfica e aceitação ponta a ponta

## Resultado esperado

Transformar a janela inicial em uma bancada visual que carregue programas,
controle a execução e permita inspecionar todo o estado relevante. Entregar
também os cenários reproduzíveis que comprovam o funcionamento da VM.

## Dependências

- `Machine` funcional do pacote 4.
- Entrada e `MachineController` do pacote 5.

O layout pode ser preparado antes, mas dados simulados não devem permanecer no
produto final.

## Ownership

Arquivos atuais:

```text
src/ui/MainWindow.cpp
src/ui/MainWindow.hpp
```

Arquivos sugeridos:

```text
src/ui/ProgramLoadDialog.*
src/ui/RegisterPanel.*
src/ui/MemoryPanel.*
src/ui/ExecutionPanel.*
src/ui/FaultPanel.*
tests/ui/
tests/acceptance/
docs/demo.md
```

Também é responsável pela revisão final do README e dos documentos existentes,
sem assumir ownership da semântica do núcleo.

## Fluxo obrigatório

1. Usuário abre o diálogo de carga.
2. Escolhe texto hexadecimal ou arquivo binário.
3. Informa endereço de carga e PC inicial.
4. Confirma uma entrada válida.
5. Inspeciona registradores, memória e instrução atual.
6. Executa um passo ou inicia execução contínua.
7. Pausa ou observa conclusão/falha.
8. Reseta ou carrega outro programa.

Entrada inválida deve permanecer no diálogo com explicação e preservar a
sessão anterior.

## Elementos da interface

- botões de carregar, step, executar, pausar e resetar;
- estado atual da execução;
- `A`, `X`, `L`, `B`, `S`, `T`, `F`, `PC` e `SW`;
- Condition Code apresentado de forma legível;
- bytes da instrução atual e opcode decodificado;
- memória consultada por endereço e faixa;
- destaque de valores alterados no último passo;
- painel persistente para falha da máquina;
- mensagens de validação de entrada próximas aos campos.

Os controles devem refletir o estado: por exemplo, pausa só durante execução e
step apenas com programa carregado e não concluído.

## Programas de aceitação

Versionar bytes já codificados, nunca assembly usado como entrada. A coleção
deve cobrir:

- carga, soma e armazenamento;
- comparação com resultados menor, igual e maior;
- direto, imediato, indireto, indexado, PC-relative e base-relative;
- salto condicional tomado e não tomado;
- `JSUB` seguido de `RSUB`;
- formato 4;
- shifts;
- opcode desconhecido;
- acesso fora da memória;
- divisão por zero.

Cada programa deve declarar bytes, endereço de carga, PC inicial, quantidade
máxima de passos e estado final esperado. O esperado deve ser escrito sem usar
a implementação para calculá-lo.

## Testes obrigatórios

- habilitação dos controles em cada estado relevante;
- validações do diálogo de carga;
- atualização dos painéis após step;
- consulta de diferentes faixas de memória;
- destaque de registradores e bytes alterados;
- apresentação dos detalhes de falha;
- pausa e retomada;
- execução de todos os programas de aceitação;
- comparação do snapshot final com o resultado esperado.

## Documentação final

Atualizar:

- README com build, execução e fluxo de uso;
- `docs/architecture.md` com o estado final;
- `docs/testing.md` com testes da UI e aceitação;
- `docs/roadmap.md` marcando a primeira etapa como concluída;
- `docs/demo.md` com roteiro da apresentação.

## Fora do escopo

- editor de assembly;
- montador, linker ou relocação;
- edição arbitrária de registradores e memória;
- produção ou edição do vídeo de apresentação;
- correção silenciosa de erros pertencentes ao núcleo.

## Critério de conclusão

- [ ] O fluxo completo pode ser executado sem terminal após a build.
- [ ] Todos os registradores e a memória são inspecionáveis.
- [ ] Controles refletem corretamente o estado da execução.
- [ ] Falhas ficam visíveis e preservam o estado para inspeção.
- [ ] Programas de aceitação cobrem instruções, modos e falhas principais.
- [ ] Testes da UI e ponta a ponta passam na CI.
- [ ] Documentação e roteiro partem de checkout limpo.
