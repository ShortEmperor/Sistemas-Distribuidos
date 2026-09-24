/*
 * matriz_mpi.c
 * Multiplicacion de matrices C = A x B distribuida con MPI.
 *
 * El proceso 0 (maestro) genera A y B, envia B completa a todos (MPI_Bcast),
 * reparte las filas de A entre los procesos (MPI_Scatterv), cada proceso
 * multiplica sus filas y el maestro junta los pedazos de C (MPI_Gatherv).
 *
 * Compilar: mpicc -O2 -o matriz_mpi matriz_mpi.c
 * Ejecutar: mpirun -np 4 --hostfile hosts ./matriz_mpi [N] [--verificar]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

static void imprimir(const char *nombre, int *M, int N)
{
    int i, j;
    printf("Matriz %s:\n", nombre);
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++)
            printf("%4d ", M[i * N + j]);
        printf("\n");
    }
}

/* C[filas x N] = A[filas x N] * B[N x N]  (orden i-k-j para usar mejor la cache) */
static void multiplicar(int *A, int *B, int *C, int filas, int N)
{
    int i, j, k;
    memset(C, 0, (size_t)filas * N * sizeof(int));
    for (i = 0; i < filas; i++)
        for (k = 0; k < N; k++) {
            int a = A[i * N + k];
            for (j = 0; j < N; j++)
                C[i * N + j] += a * B[k * N + j];
        }
}

int main(int argc, char *argv[])
{
    int rank, size, i, p;
    int N = 1000, verificar = 0;
    int *A = NULL, *B, *C = NULL, *A_local, *C_local;
    int *filas, *inicio, *conteos, *desplazamientos;
    char host[MPI_MAX_PROCESSOR_NAME];
    int largo_host;
    double t0, t_calc, t_total;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(host, &largo_host);

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verificar") == 0)
            verificar = 1;
        else
            N = atoi(argv[i]);
    }
    if (N <= 0) {
        if (rank == 0)
            fprintf(stderr, "Uso: %s [N] [--verificar]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    /* cuantas filas le tocan a cada proceso
     * si N no es divisible entre size, los primeros reciben una fila extra */
    filas = malloc(size * sizeof(int));
    inicio = malloc(size * sizeof(int));
    conteos = malloc(size * sizeof(int));
    desplazamientos = malloc(size * sizeof(int));
    for (p = 0; p < size; p++) {
        filas[p] = N / size + (p < N % size ? 1 : 0);
        inicio[p] = (p == 0) ? 0 : inicio[p - 1] + filas[p - 1];
        conteos[p] = filas[p] * N;
        desplazamientos[p] = inicio[p] * N;
    }

    B = malloc((size_t)N * N * sizeof(int));
    A_local = malloc((size_t)(filas[rank] > 0 ? filas[rank] : 1) * N * sizeof(int));
    C_local = malloc((size_t)(filas[rank] > 0 ? filas[rank] : 1) * N * sizeof(int));
    if (B == NULL || A_local == NULL || C_local == NULL) {
        fprintf(stderr, "Proceso %d en %s: sin memoria para N=%d\n", rank, host, N);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* el maestro genera las matrices (numeros del 0 al 9) */
    if (rank == 0) {
        A = malloc((size_t)N * N * sizeof(int));
        C = malloc((size_t)N * N * sizeof(int));
        if (A == NULL || C == NULL) {
            fprintf(stderr, "Maestro: sin memoria para N=%d\n", N);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        srand(42);
        for (i = 0; i < N * N; i++) {
            A[i] = rand() % 10;
            B[i] = rand() % 10;
        }
        printf("Multiplicando matrices de %dx%d con %d procesos\n", N, N, size);
        if (N <= 10) {
            imprimir("A", A, N);
            imprimir("B", B, N);
        }
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();

    /* 1) todos reciben B completa */
    MPI_Bcast(B, N * N, MPI_INT, 0, MPI_COMM_WORLD);

    /* 2) cada proceso recibe solo sus filas de A */
    MPI_Scatterv(A, conteos, desplazamientos, MPI_INT,
                 A_local, conteos[rank], MPI_INT, 0, MPI_COMM_WORLD);

    /* 3) cada proceso calcula su parte de C */
    t_calc = MPI_Wtime();
    multiplicar(A_local, B, C_local, filas[rank], N);
    t_calc = MPI_Wtime() - t_calc;

    if (filas[rank] > 0)
        printf("Proceso %d en %s: filas %d a %d (%.3f s de calculo)\n",
               rank, host, inicio[rank], inicio[rank] + filas[rank] - 1, t_calc);
    else
        printf("Proceso %d en %s: sin filas (N < procesos)\n", rank, host);
    fflush(stdout);

    /* 4) el maestro junta los resultados */
    MPI_Gatherv(C_local, conteos[rank], MPI_INT,
                C, conteos, desplazamientos, MPI_INT, 0, MPI_COMM_WORLD);

    t_total = MPI_Wtime() - t0;

    if (rank == 0) {
        printf("Tiempo total (envio + calculo + recoleccion): %.3f s\n", t_total);
        if (N <= 10)
            imprimir("C", C, N);

        if (verificar) {
            /* se recalcula todo en un solo proceso y se compara */
            int *C_serial = malloc((size_t)N * N * sizeof(int));
            double t_serial = MPI_Wtime();
            multiplicar(A, B, C_serial, N, N);
            t_serial = MPI_Wtime() - t_serial;
            int correcto = memcmp(C, C_serial, (size_t)N * N * sizeof(int)) == 0;
            printf("Verificacion contra version secuencial: %s\n",
                   correcto ? "CORRECTO" : "ERROR");
            printf("Tiempo secuencial: %.3f s  ->  speedup: %.2fx\n",
                   t_serial, t_serial / t_total);
            free(C_serial);
        }
        free(A);
        free(C);
    }

    free(B); free(A_local); free(C_local);
    free(filas); free(inicio); free(conteos); free(desplazamientos);
    MPI_Finalize();
    return 0;
}
