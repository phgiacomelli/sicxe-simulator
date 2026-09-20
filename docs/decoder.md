# Decoder de instruções

## Responsabilidade

`Decoder` transforma os bytes armazenados na memória em uma
`DecodedInstruction`. Ele reconhece a operação, determina o formato e extrai
os campos codificados, mas não interpreta o endereço final do operando.

Sua interface pública é:

```cpp
DecodeResult decode(const Memory& memory, std::uint32_t address) const;
```

O resultado é uma `std::variant` contendo `DecodedInstruction` ou
`DecodeFault`.

## Conjunto reconhecido

O simulador reconhece somente as 38 instruções que serão implementadas.

### Formato 2

`ADDR`, `CLEAR`, `COMPR`, `DIVR`, `MULR`, `RMO`, `SHIFTL`, `SHIFTR`, `SUBR` e
`TIXR`.

### Formatos 3 e 4

`ADD`, `AND`, `COMP`, `DIV`, `J`, `JEQ`, `JGT`, `JLT`, `JSUB`, `LDA`, `LDB`,
`LDCH`, `LDL`, `LDS`, `LDT`, `LDX`, `MUL`, `OR`, `RSUB`, `STA`, `STB`, `STCH`,
`STL`, `STS`, `STT`, `STX`, `SUB` e `TIX`.

Qualquer opcode fora desse conjunto produz `UnknownOpcode`. O decoder não
mantém uma lista separada de instruções que não fazem parte do projeto.

## Resultado bem-sucedido

`DecodedInstruction` contém:

- `opcode`: operação reconhecida;
- `format`: SIC, 2, 3 ou 4;
- `instructionAddress`: endereço do primeiro byte;
- `length`: quantidade de bytes da instrução;
- `rawBytes`: até quatro bytes lidos;
- `r1` e `r2`: campos do formato 2;
- `n`, `i`, `x`, `b`, `p` e `e`: flags dos formatos de memória;
- `encodedOperand`: endereço ou deslocamento ainda não resolvido.

`encodedOperand` é deliberadamente um valor codificado. Aplicar PC, `B`, `X`
ou indireção será responsabilidade do `AddressResolver`.

## Formato SIC

Quando `n=0` e `i=0`, os 15 bits restantes são interpretados como a codificação
SIC legada:

- o bit mais significativo do segundo byte é `x`;
- os outros 15 bits formam o endereço;
- as flags `b`, `p` e `e` não são interpretadas separadamente.

Somente instruções compatíveis com essa codificação são aceitas.

## Formato 2

O primeiro byte contém o opcode e o segundo contém dois nibbles.

- `CLEAR` e `TIXR` usam apenas `r1`; o segundo nibble deve ser zero.
- `SHIFTL` e `SHIFTR` usam `r1` e preservam o segundo nibble como contagem
  codificada. A execução aplicará o ajuste necessário.
- As demais instruções de formato 2 usam `r1` e `r2`.

Os códigos aceitos pelo decoder são `A=0`, `X=1`, `L=2`, `B=3`, `S=4`, `T=5`,
`PC=8` e `SW=9`. O registrador `F=6` é rejeitado porque as instruções de ponto
flutuante não fazem parte do recorte implementado pelo simulador. Outros códigos
também são rejeitados para as instruções implementadas.

## Formatos 3 e 4

As combinações de `n` e `i` representam:

| `n` | `i` | Interpretação |
| --- | --- | --- |
| 0 | 0 | SIC legado |
| 0 | 1 | Imediato |
| 1 | 0 | Indireto |
| 1 | 1 | Simples |

Regras validadas pelo decoder:

- `b` e `p` não podem estar ativos simultaneamente;
- formato 4 (`e=1`) não aceita `b` ou `p`;
- indexação (`x=1`) é aceita somente no modo simples;
- formato 3 possui campo de 12 bits;
- formato 4 possui campo de 20 bits.

O decoder apenas preserva esses campos. Extensão de sinal do deslocamento
relativo ao PC e somas com registradores pertencem ao resolver.

## Caso especial de `RSUB`

`RSUB` não possui operando. São aceitas as codificações sem endereço e sem
flags de endereçamento adicionais nos formatos SIC e XE de três bytes.

Codificações com endereço, indexação, relatividade ou formato 4 são rejeitadas.

## Falhas

`DecodeFault` registra:

- o código da falha;
- o endereço da instrução;
- os bytes lidos;
- a quantidade de bytes válidos.

Os códigos atuais são:

| Código | Significado |
| --- | --- |
| `UnknownOpcode` | O primeiro byte não identifica uma instrução reconhecida |
| `TruncatedInstruction` | A memória terminou antes de todos os bytes necessários |
| `InvalidRegister` | Um campo de formato 2 não identifica um registrador permitido |
| `InvalidFlagCombination` | As flags `n/i/x/b/p/e` formam uma combinação inválida |
| `InvalidOperandEncoding` | Um campo reservado ou sem uso contém valor inválido |

## Exemplos

### Imediato, formato 3

```text
01 00 05
```

Resultado relevante:

```text
opcode = LDA
format = Format3
n = 0, i = 1
encodedOperand = 0x005
```

### `JSUB`, formato 4

```text
4B 10 10 00
```

Resultado relevante:

```text
opcode = JSUB
format = Format4
length = 4
encodedOperand = 0x01000
```

### SIC indexado

```text
00 80 10
```

Resultado relevante:

```text
opcode = LDA
format = Sic
x = 1
encodedOperand = 0x0010
```

## O que não pertence ao decoder

- calcular o endereço efetivo;
- aplicar o PC ou o registrador base;
- somar o registrador `X`;
- seguir um ponteiro indireto;
- ler o valor final do operando;
- executar a operação;
- modificar PC, memória ou registradores.
