struct matrices1 {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[10000];
    int B[10000];
};

struct resultado1 {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[10000];
};

program MATRIZ_PROG1 {
    version MATRIZ_VERS {
        resultado1 MULTIPLICAR1(matrices1) = 1;
    } = 1;
} = 0x20000001;
