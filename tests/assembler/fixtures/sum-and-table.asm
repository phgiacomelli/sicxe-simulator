. Sum 1..5, double through a subroutine, then add TABLE (7): RESULT = 37.
DEMO   START 0x100
ENTRY  CLEAR X
       LDA #0
       LDS #5
LOOP   TIXR S
       ADDR X,A
       JLT LOOP
       STA SUM
       +JSUB DOUBLE
       STA RESULT
       +LDB #TABLE
       BASE TABLE
       LDA TABLE
       NOBASE
       ADD RESULT
       STA RESULT
DONE   J DONE
SUM    WORD 0
RESULT RESW 1
DOUBLE LDA SUM
       MUL #2
       RSUB
GAP    RESB 4096
TABLE  WORD 7
PTR    WORD SUM
CHAR   BYTE C'OK'
       END ENTRY
