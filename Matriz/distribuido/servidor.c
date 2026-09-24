/*
 * servidor.c - multiplica las filas de A que le mandan por B.
 * Stubs generados con rpcgen -M (el resultado lo llena el servidor en *res
 * y rpcgen lo libera despues con matriz_prog_1_freeresult).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "matriz.h"

static double ahora(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

bool_t multiplicar_1_svc(peticion *p, respuesta *res, struct svc_req *req)
{
    static char host[64];
    int n = p->n;
    int filas = p->fila_fin - p->fila_inicio;
    int i, j, k;
    double t0;

    gethostname(host, sizeof(host));
    printf("[%s] recibi filas %d a %d (N=%d)\n", host, p->fila_inicio, p->fila_fin - 1, n);

    /* validar que llego lo que se esperaba */
    if (filas <= 0 || p->A.A_len != (u_int)filas * n || p->B.B_len != (u_int)n * n) {
        printf("[%s] peticion invalida: A=%u B=%u\n", host, p->A.A_len, p->B.B_len);
        return FALSE;
    }

    res->fila_inicio = p->fila_inicio;
    res->fila_fin = p->fila_fin;
    res->C.C_len = filas * n;
    res->C.C_val = calloc((size_t)filas * n, sizeof(int));
    res->servidor = strdup(host);
    if (res->C.C_val == NULL) {
        printf("[%s] sin memoria para %d filas\n", host, filas);
        return FALSE;
    }

    /* C_local = A_local x B  (orden i-k-j para usar mejor la cache) */
    t0 = ahora();
    for (i = 0; i < filas; i++)
        for (k = 0; k < n; k++) {
            int a = p->A.A_val[i * n + k];
            int *fila_b = &p->B.B_val[k * n];
            int *fila_c = &res->C.C_val[i * n];
            for (j = 0; j < n; j++)
                fila_c[j] += a * fila_b[j];
        }
    res->segundos = ahora() - t0;

    printf("[%s] calcule filas %d a %d en %.3f s\n",
           host, p->fila_inicio, p->fila_fin - 1, res->segundos);
    return TRUE;
}

int matriz_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
    xdr_free(xdr_result, result);
    return 1;
}
