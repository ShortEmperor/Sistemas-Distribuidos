#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "matriz.h"

#define N 2
#define Serv 2

int main()
{
    CLIENT *clnt;
    matrices m;
    resultado *res;

    int i, j, s;
    int sock = RPC_ANYSOCK;

    struct sockaddr_in addr;
    struct timeval timeout;

    int puertos[Serv] = {5001, 5002};

    memset(&m, 0, sizeof(m));
    m.n = N;

    // 🔹 llenar matrices
    printf("Matriz A:\n");
    for(i=0;i<N;i++)
    {
        for(j=0;j<N;j++)
        {
            m.A[i*N+j] = i + j + 1;
            printf("%d ", m.A[i*N+j]);
        }
        printf("\n");
    }

    printf("\nMatriz B:\n");
    for(i=0;i<N;i++)
    {
        for(j=0;j<N;j++)
        {
            m.B[i*N+j] = (i+1)*(j+1);
            printf("%d ", m.B[i*N+j]);
        }
        printf("\n");
    }

    int bloque = N / Serv;

    for(s = 0; s < Serv; s++)
    {
        m.fila_inicio = s * bloque;
        m.fila_fin = (s + 1) * bloque;

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(puertos[s]);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        clnt = clntudp_create(&addr, MATRIZ_PROG, MATRIZ_VERS, timeout, &sock);

        if(clnt == NULL)
        {
            clnt_pcreateerror("Error creando cliente");
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
