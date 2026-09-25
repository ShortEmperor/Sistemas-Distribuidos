/*
 * Servidor 1 (MATRIZ_PROG1).
 * Antes este archivo no coincidia con matriz.h (tipos matrices/resultado con A[4]
 * y MATRIZ_PROG definido dos veces). Ahora usa los tipos que usan cliente1.c y
 * servidor1.c, con arreglos para matrices de hasta 1500 x 1500.
 */

const MAX_N = 1500;
const MAX_ELEM = 2250000;   /* MAX_N * MAX_N */

struct matrices1 {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[MAX_ELEM];
    int B[MAX_ELEM];
};

struct resultado1 {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[MAX_ELEM];
};

program MATRIZ_PROG1 {
    version MATRIZ_VERS {
        resultado1 MULTIPLICAR1(matrices1) = 1;
    } = 1;
} = 0x20000001;
