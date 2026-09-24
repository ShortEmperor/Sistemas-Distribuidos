/*
 * Multiplicacion de matrices distribuida con RPC.
 * Todos los servidores tienen el mismo programa; el cliente le manda a cada
 * uno solo las filas de A que le tocan y la matriz B completa.
 *
 * Arreglos de tamaño variable (<>) en lugar de fijos ([100], [10000]):
 * asi el mismo programa sirve para cualquier N y solo viaja lo necesario.
 */

struct peticion {
    int n;              /* tamaño de la matriz (N x N) */
    int fila_inicio;    /* primera fila que calcula este servidor */
    int fila_fin;       /* ultima fila + 1 */
    int A<>;            /* solo las filas [fila_inicio, fila_fin) de A */
    int B<>;            /* B completa (N x N) */
};

struct respuesta {
    int fila_inicio;
    int fila_fin;
    int C<>;            /* filas [fila_inicio, fila_fin) de C */
    double segundos;    /* tiempo de calculo en el servidor */
    string servidor<64>; /* hostname del servidor que calculo */
};

program MATRIZ_PROG {
    version MATRIZ_VERS {
        respuesta MULTIPLICAR(peticion) = 1;
    } = 1;
} = 0x20000101;
