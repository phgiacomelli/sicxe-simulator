# Demonstração da primeira etapa — VM

Este roteiro começa com um checkout limpo. Instale as dependências descritas no
[README](../README.md), configure, compile e execute:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/SICXESimulator
```

Os bytes abaixo já são código de máquina. Não digite assembly na interface:
ela não contém montador, linker ou loader de formato objeto.

## Carga, soma e armazenamento

1. Clique em **Carregar Programa** e mantenha **Texto Hexadecimal**.
2. Cole `01 00 05 19 00 07 0F 00 20`.
3. Informe endereço de carga `0` e PC inicial `0`; confirme.
4. Observe a instrução atual `LDA`, bytes `01 00 05`, no PC `000000`.
5. Clique **Executar Passo** três vezes. Após o primeiro passo, `A=000005` e
   `PC=000003`; após o segundo, `A=00000C` e `PC=000006`; após o terceiro,
   `PC=000009` e a memória em `000020..000022` contém `00 00 0C`.
6. No painel de memória, busque endereço `20` e faixa de 16 bytes. O byte
   `0C` aparece destacado por ter sido escrito no último passo.
7. Clique **Reset** e observe memória e registradores zerados.

Não existe `HALT`: o PC chegar a `000009` não encerra o programa. Os bytes
seguintes ainda pertencem à memória e poderiam ser interpretados como
instruções. Pare no passo previsto, pause ou resete antes de continuar.

## Falha e preservação da sessão

1. Carregue `FF` nos endereços de carga e PC `7`; execute um passo.
2. O painel de diagnóstico deve mostrar `UnknownOpcode`, PC `000007` e byte
   `FF`. O estado fica disponível para inspeção; `Step` e `Executar` ficam
   desabilitados até reset ou nova carga.
3. Tente carregar `01 00 05` com PC inicial `100000`. O diálogo deve permanecer
   aberto com erro junto ao campo PC. Cancele: a sessão anterior e sua falha
   permanecem intactas.

## Execução contínua e pausa

Carregue `19 00 01 3F 00 00` em `0`/`0`, clique **Executar**, depois **Pausar**.
O acumulador cresce a cada passagem pelo loop; a pausa ocorre entre
instruções. Clique **Executar** novamente para retomar e **Pausar** para parar.
Durante a execução, carga e reset ficam desabilitados; após pausar, `Reset`
limpa o estado. Os testes automatizados cobrem os demais programas
codificados em `tests/acceptance/ProgramAcceptanceTest.cpp`.
