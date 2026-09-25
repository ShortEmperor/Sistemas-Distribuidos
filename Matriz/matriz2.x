/*
 * Servidor 2 (MATRIZ_PROG2).
 * Antes los arreglos eran de 1024 y no coincidian con matriz2.h (de 100).
 * Ahora son para matrices de hasta 1500 x 1500, igual que matriz.x.
 */

const MAX_N = 1500;
const MAX_ELEM = 2250000;   /* MAX_N * MAX_N */

struct matrices2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int A[MAX_ELEM];
    int B[MAX_ELEM];
};

struct resultado2 {
    int n;
    int fila_inicio;
    int fila_fin;
    int C[MAX_ELEM];
};

program MATRIZ_PROG2 {
    version MATRIZ_VERS {
        resultado2 MULTIPLICAR2(matrices2) = 1;
    } = 1;
} = 0x20000002;
