#include <stdio.h>
#include "matriz.h"

resultado *multiplicar1_1_svc(matrices *m, struct svc_req *req)
{
    static resultado res;

    int i,j,k,n;

    n = m->n;

    res.n = n;
    res.fila_inicio = m->fila_inicio;
    res.fila_fin = m->fila_fin;

    for(i=m->fila_inicio;i<m->fila_fin;i++)
    {
        for(j=0;j<n;j++)
        {
            res.C[i*n+j]=0;

            for(k=0;k<n;k++)
            {
                res.C[i*n+j]+=m->A[i*n+k]*m->B[k*n+j];
            }
        }
    }

    printf("Servidor 1 calculó filas %d a %d\n",
           m->fila_inicio,m->fila_fin-1);

    return &res;
}
