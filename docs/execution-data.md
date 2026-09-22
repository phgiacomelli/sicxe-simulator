# Operações de Transferência e Lógica

Este documento detalha o comportamento do módulo responsável por instruções de 
transferência de dados e operações lógicas (`CLEAR`, `RMO`, `LDA`, `LDB`, `LDL`, 
`LDS`, `LDT`, `LDX`, `STA`, `STB`, `STL`, `STS`, `STT`, `STX`, `LDCH`, `STCH`, 
`AND`, `OR`).

## Matriz Fonte / Destino / Largura

Conforme os critérios do Pacote 2, a tabela abaixo mapeia a largura dos efeitos 
produzidos por cada instrução[cite: 5]:

| Instrução | Fonte | Destino | Largura (bits) | Observações |
| :--- | :--- | :--- | :--- | :--- |
| `CLEAR` | N/A | Registrador (r1) | 24 | Zera o registrador[cite: 5]. |
| `RMO` | Registrador (r1) | Registrador (r2) | 24 | |
| `LD*` | Memória/Imediato | Registrador | 24 | Copia exatamente 24 bits[cite: 5]. |
| `ST*` | Registrador | Memória | 24 | Escrita de 3 bytes (big-endian)[cite: 5]. |
| `LDCH` | Memória/Imediato | Registrador (A) | 8 | Substitui apenas os 8 bits menos significativos[cite: 5]. |
| `STCH` | Registrador (A) | Memória | 8 | Grava apenas os 8 bits menos significativos[cite: 5]. |
| `AND`/`OR` | Memória/Imediato | Registrador | 24 | Operam sobre padrões de 24 bits[cite: 5]. |

Nenhuma destas operações altera o Condition Code (CC) e o cálculo de endereços não 
ocorre dentro deste módulo[cite: 5]. Combinações incompatíveis para operações de `ST*` 
resultam em falha estruturada (`ExecuteFault`), sem lançar exceções para o chamador[cite: 5].