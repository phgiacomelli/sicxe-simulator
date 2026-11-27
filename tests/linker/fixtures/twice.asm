LIB     START 0x400
        EXTDEF TWICE
        EXTREF VALUE
TWICE   +LDA VALUE
        ADDR A,A
        RSUB
        END
