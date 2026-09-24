struct matrices2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[10000];
    int B[10000];
};

struct resultado2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[10000];
};

program MATRIZ_PROG2 {
    version MATRIZ_VERS {
        resultado2 MULTIPLICAR2(matrices2) = 1;
    } = 1;
} = 0x20000002;
