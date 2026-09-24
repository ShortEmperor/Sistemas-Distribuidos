struct matrices2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[1024];
    int B[1024];
};

struct resultado2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[1024];
};

program MATRIZ_PROG2 {
    version MATRIZ_VERS {
        resultado2 MULTIPLICAR2(matrices2) = 1;
    } = 1;
} = 0x20000002;
