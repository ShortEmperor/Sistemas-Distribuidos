#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matriz.h"  
#include "matriz2.h"

#define SERV 2
// antes "#define N 32": 32x32 = 1024 no cabe en A[100] de matriz.h y el cliente se colgaba.
// Ahora N se puede pasar como tercer argumento (por defecto 10) y se valida contra
// el tamaño de los arreglos.
#define N_DEFECTO 10

int main(int argc, char *argv[])
{
    int N = N_DEFECTO;
    CLIENT *clnt;
    //matriz para cada servidor
    matrices1 m1; 
    matrices2 m2;
    
    //punteros del resultado c/u
    resultado1 *res1;
    resultado2 *res2;

    int i, j, s;
    char *servidores[SERV] = {"localhost", "localhost"};
    // ./cliente1 host_servidor1 host_servidor2 [N]  (sin argumentos usa localhost y N=10)
    if(argc >= 3) {
        servidores[0] = argv[1];
        servidores[1] = argv[2];
    }
    int programas[SERV] = {MATRIZ_PROG1, MATRIZ_PROG2};
    int C[1024];
    memset(C, 0, sizeof(C));

    // N maximo: el mas grande con N*N <= elementos del arreglo mas chico (A de matriz.h,
    // A de matriz2.h o C). Con A[100] da 10.
    int elementos = sizeof(m1.A) / sizeof(int);
    if(sizeof(m2.A) / sizeof(int) < elementos) elementos = sizeof(m2.A) / sizeof(int);
    if(sizeof(C) / sizeof(int) < elementos) elementos = sizeof(C) / sizeof(int);
    int n_max = 1;
    while((n_max + 1) * (n_max + 1) <= elementos) n_max++;

    if(argc >= 4) {
        N = atoi(argv[3]);
        if(N < 1 || N > n_max) {
            printf("N debe estar entre 1 y %d: los arreglos de matriz.h/matriz2.h son de %d elementos\n",
                   n_max, elementos);
            return 1;
        }
    }

    // inicializamos matriz 
    m1.n = N;
    printf("Matriz A:\n");
    for(i = 0; i < N; i++) {
        for(j = 0; j < N; j++) {
            m1.A[i*N + j] = 1; //i + j + 1;
            printf("%d ", m1.A[i*N + j]);
        }
        printf("\n");
    }

    printf("\nMatriz B:\n");
    for(i = 0; i < N; i++) {
        for(j = 0; j < N; j++) {
            m1.B[i*N + j] = 1; //(i+1)*(j+1);
            printf("%d ", m1.B[i*N + j]);
        }
        printf("\n");
    }

    //llamamod a servidores rpc
    int bloque = N / SERV;

    for(s = 0; s < SERV; s++)
    {
        clnt = clnt_create(servidores[s], programas[s], MATRIZ_VERS, "udp");
        if(clnt == NULL) {
            clnt_pcreateerror(servidores[s]);
            exit(1);
        }

        if(s == 0) {
            // configuramos m1 para el Servidor 1
            m1.fila_inicio = 0;
            m1.fila_fin = bloque;
            
            res1 = multiplicar1_1(&m1, clnt);
            
            if(res1 == NULL) {
                clnt_perror(clnt, "Error en Servidor 1");
                exit(1);
            }

            printf("\nResultado parcial servidor 1:\n");
            for(i = m1.fila_inicio; i < m1.fila_fin; i++) {
                for(j = 0; j < N; j++) {
                    printf("%d ", res1->C[i*N + j]);
                    C[i*N + j] = res1->C[i*N + j];
                }
                printf("\n");
            }
        } 
        else {
            // configuramos m2 para el Servidor 2 (copia de m1 a m2)
            m2.n = N;
            m2.fila_inicio = bloque;
            m2.fila_fin = N;
            memcpy(m2.A, m1.A, sizeof(m2.A));
            memcpy(m2.B, m1.B, sizeof(m2.B));

            res2 = multiplicar2_1(&m2, clnt);

            if(res2 == NULL) {
                clnt_perror(clnt, "Error en Servidor 2");
                exit(1);
            }

            printf("\nResultado parcial servidor 2:\n");
            for(i = m2.fila_inicio; i < m2.fila_fin; i++) {
                for(j = 0; j < N; j++) {
                    printf("%d ", res2->C[i*N + j]);
                    C[i*N + j] = res2->C[i*N + j];
                }
                printf("\n");
            }
        }
        clnt_destroy(clnt);
    }

    //matriz final
    printf("\nMatriz resultado C completa:\n");
    for(i = 0; i < N; i++) {
        for(j = 0; j < N; j++) {
            printf("%d ", C[i*N + j]);
        }
        printf("\n");
    }

    return 0;
}
