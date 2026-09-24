/*
 * cliente.c - multiplicacion de matrices distribuida con RPC
 *
 *   ./cliente N [--verificar] servidor1 servidor2 ... servidorK
 *
 * Reparte las filas de A entre los K servidores, les manda B completa y
 * llama a todos AL MISMO TIEMPO (un hilo por servidor), luego junta C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "matriz.h"

typedef struct {
    char *host;
    int n, fila_inicio, fila_fin;
    int *A, *B, *C;         /* matrices completas del cliente */
    int ok;
    double segundos_servidor, segundos_total;
    char servidor[64];
} trabajo;

static double ahora(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static void multiplicar_local(int *A, int *B, int *C, int filas, int n)
{
    int i, j, k;
    memset(C, 0, (size_t)filas * n * sizeof(int));
    for (i = 0; i < filas; i++)
        for (k = 0; k < n; k++) {
            int a = A[i * n + k];
            for (j = 0; j < n; j++)
                C[i * n + j] += a * B[k * n + j];
        }
}

static void imprimir(const char *nombre, int *M, int n)
{
    int i, j;
    printf("Matriz %s:\n", nombre);
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++)
            printf("%4d ", M[i * n + j]);
        printf("\n");
    }
}

static void *llamar_servidor(void *arg)
{
    trabajo *t = arg;
    CLIENT *clnt;
    peticion p;
    respuesta res;
    struct timeval espera = { 3600, 0 };   /* 10000x10000 tarda minutos */
    int filas = t->fila_fin - t->fila_inicio;
    double t0 = ahora();

    t->ok = 0;
    clnt = clnt_create(t->host, MATRIZ_PROG, MATRIZ_VERS, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(t->host);
        return NULL;
    }
    clnt_control(clnt, CLSET_TIMEOUT, (char *)&espera);

    p.n = t->n;
    p.fila_inicio = t->fila_inicio;
    p.fila_fin = t->fila_fin;
    p.A.A_len = filas * t->n;                       /* solo sus filas de A */
    p.A.A_val = &t->A[(size_t)t->fila_inicio * t->n];
    p.B.B_len = t->n * t->n;                        /* B completa */
    p.B.B_val = t->B;

    memset(&res, 0, sizeof(res));
    if (multiplicar_1(&p, &res, clnt) != RPC_SUCCESS) {
        clnt_perror(clnt, t->host);
        clnt_destroy(clnt);
        return NULL;
    }

    if (res.C.C_len == (u_int)filas * t->n) {
        memcpy(&t->C[(size_t)t->fila_inicio * t->n], res.C.C_val,
               (size_t)res.C.C_len * sizeof(int));
        t->segundos_servidor = res.segundos;
        snprintf(t->servidor, sizeof(t->servidor), "%s", res.servidor);
        t->ok = 1;
    } else {
        fprintf(stderr, "%s: respuesta con tamaño incorrecto\n", t->host);
    }

    xdr_free((xdrproc_t)xdr_respuesta, (char *)&res);
    clnt_destroy(clnt);
    t->segundos_total = ahora() - t0;
    return NULL;
}

int main(int argc, char *argv[])
{
    int n = 0, verificar = 0, k = 0, s, i;
    char **hosts = malloc(argc * sizeof(char *));
    int *A, *B, *C;
    trabajo *trabajos;
    pthread_t *hilos;
    double t0, t_total;
    int fallas = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verificar") == 0)
            verificar = 1;
        else if (n == 0 && atoi(argv[i]) > 0)
            n = atoi(argv[i]);
        else
            hosts[k++] = argv[i];
    }
    if (n <= 0 || k == 0) {
        printf("Uso: %s N [--verificar] servidor1 servidor2 ... servidorK\n", argv[0]);
        return 1;
    }

    A = malloc((size_t)n * n * sizeof(int));
    B = malloc((size_t)n * n * sizeof(int));
    C = malloc((size_t)n * n * sizeof(int));
    if (A == NULL || B == NULL || C == NULL) {
        printf("Sin memoria para N=%d\n", n);
        return 1;
    }

    srand(42);
    for (i = 0; i < n * n; i++) {
        A[i] = rand() % 10;
        B[i] = rand() % 10;
    }
    printf("Multiplicando matrices de %dx%d con %d servidores\n", n, n, k);
    if (n <= 10) {
        imprimir("A", A, n);
        imprimir("B", B, n);
    }

    /* repartir filas: si N no es divisible entre K, los primeros llevan una extra */
    trabajos = calloc(k, sizeof(trabajo));
    hilos = calloc(k, sizeof(pthread_t));
    int inicio = 0;
    for (s = 0; s < k; s++) {
        int filas = n / k + (s < n % k ? 1 : 0);
        trabajos[s].host = hosts[s];
        trabajos[s].n = n;
        trabajos[s].fila_inicio = inicio;
        trabajos[s].fila_fin = inicio + filas;
        trabajos[s].A = A;
        trabajos[s].B = B;
        trabajos[s].C = C;
        inicio += filas;
    }

    /* llamar a todos los servidores en paralelo */
    t0 = ahora();
    for (s = 0; s < k; s++)
        if (trabajos[s].fila_fin > trabajos[s].fila_inicio)
            pthread_create(&hilos[s], NULL, llamar_servidor, &trabajos[s]);
    for (s = 0; s < k; s++)
        if (trabajos[s].fila_fin > trabajos[s].fila_inicio)
            pthread_join(hilos[s], NULL);
    t_total = ahora() - t0;

    for (s = 0; s < k; s++) {
        trabajo *t = &trabajos[s];
        if (t->fila_fin == t->fila_inicio)
            printf("%s: sin filas (N < servidores)\n", t->host);
        else if (t->ok)
            printf("%s (%s): filas %d a %d, calculo %.3f s, total con red %.3f s\n",
                   t->host, t->servidor, t->fila_inicio, t->fila_fin - 1,
                   t->segundos_servidor, t->segundos_total);
        else {
            printf("%s: FALLO filas %d a %d\n", t->host, t->fila_inicio, t->fila_fin - 1);
            fallas++;
        }
    }
    printf("Tiempo total (envio + calculo + recoleccion): %.3f s\n", t_total);
    if (fallas > 0) {
        printf("%d servidor(es) fallaron, el resultado esta incompleto\n", fallas);
        return 1;
    }

    if (n <= 10)
        imprimir("C", C, n);

    if (verificar) {
        int *C_local = malloc((size_t)n * n * sizeof(int));
        double t_local = ahora();
        multiplicar_local(A, B, C_local, n, n);
        t_local = ahora() - t_local;
        printf("Verificacion contra version local: %s\n",
               memcmp(C, C_local, (size_t)n * n * sizeof(int)) == 0 ? "CORRECTO" : "ERROR");
        printf("Tiempo local (1 maquina): %.3f s  ->  speedup: %.2fx\n",
               t_local, t_local / t_total);
        free(C_local);
    }

    free(A); free(B); free(C); free(trabajos); free(hilos); free(hosts);
    return 0;
}
