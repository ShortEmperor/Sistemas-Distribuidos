#include <stdio.h>
#include <stdlib.h>
#include "matriz.h"
#include <string.h>
#define Serv 2
#define N 4

int main(int argc, char *argv[])
{
    CLIENT *clnt;
    matrices m;
    resultado *res;

    int i, j, s;

    char *servidores[Serv] = {
        "localhost",
	"localhost"
	/*"192.168.229.48",
        "192.168.229.50",
        "192.168.1.13",
        "192.168.1.14"*/
    };

    memset(&m, 0, sizeof(m));

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

    int bloque = N / Serv;
    int programas[Serv] = {MATRIZ_PROG, MATRIZ_PROG2};

    for(s = 0; s < Serv; s++)
    {
        m.fila_inicio = s * bloque;
        m.fila_fin = (s + 1) * bloque;

        clnt = clnt_create(
            servidores[s],
            programas[s],   
            MATRIZ_VERS,
            "udp"
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
