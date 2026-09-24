#include <stdio.h>
#include <stdlib.h>
#include "matriz.h"

#define N 4
#define SERV 4

int main(int argc, char *argv[])
{
    CLIENT *clnt;
    matrices m;
    resultado *res;

    int i, j, s;

    // antes era servidores[2] pero el ciclo va de 0 a 3:
    // servidores[2] y servidores[3] eran basura -> Segmentation fault
    char *servidores[SERV] = {
        "192.168.229.48",
        "192.168.229.50",
        "192.168.1.13",
        "192.168.1.14"
    };

    // ./cliente host1 host2 host3 host4  (sin argumentos usa las IPs de arriba)
    if(argc > SERV)
    {
        for(s = 0; s < SERV; s++)
            servidores[s] = argv[s + 1];
    }

    m.n = N;

    printf("Matriz A:\n");

    for(i = 0; i < N; i++)
    {
        for(j = 0; j < N; j++)
        {
            m.A[i*N + j] = i + j + 1;
            printf("%d ", m.A[i*N + j]);
        }
        printf("\n");
    }

    printf("\nMatriz B:\n");

    for(i = 0; i < N; i++)
    {
        for(j = 0; j < N; j++)
        {
            m.B[i*N + j] = (i+1)*(j+1);
            printf("%d ", m.B[i*N + j]);
        }
        printf("\n");
    }

    int bloque = N / SERV;

    for(s = 0; s < SERV; s++)
    {
        m.fila_inicio = s * bloque;
        m.fila_fin = (s + 1) * bloque;
        // si N no es divisible entre SERV, el ultimo servidor hace las filas que sobran
        if(s == SERV - 1)
            m.fila_fin = N;

        // antes "udp": A[10000] y B[10000] siempre viajan completos (80 KB)
        // aunque N sea 4, no caben en UDP -> "RPC: Can't encode arguments"
        clnt = clnt_create(
            servidores[s],
            MATRIZ_PROG,
            MATRIZ_VERS,
            "tcp"
        );

        if(clnt == NULL)
        {
            clnt_pcreateerror(servidores[s]);
            exit(1);
        }

        res = multiplicar_1(&m, clnt);

        if(res == NULL)
        {
            clnt_perror(clnt, "Error en RPC");
            exit(1);
        }

        printf("\nResultado parcial servidor %d:\n", s+1);

        for(i = m.fila_inicio; i < m.fila_fin; i++)
        {
            for(j = 0; j < N; j++)
            {
                printf("%d ", res->C[i*N + j]);
            }
            printf("\n");
        }

        clnt_destroy(clnt);
    }

    return 0;
}
