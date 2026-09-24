struct matrices {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[10000];
    int B[10000];
};

struct resultado {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[10000];
};

program MATRIZ_PROG {
    version MATRIZ_VERS {
        resultado MULTIPLICAR(matrices) = 1;
    } = 1;
} = 0x20000001;
