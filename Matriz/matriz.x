struct matrices {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[4];
    int B[4];
};

struct resultado {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[4];
};

program MATRIZ_PROG {
    version MATRIZ_VERS {
        resultado MULTIPLICAR1(matrices) = 1;
    } = 1;
} = 0x20000001;

program MATRIZ_PROG {
    version MATRIZ_VERS {
        resultado MULTIPLICAR2(matrices) = 1;
    } = 1;
} = 0x20000002;

