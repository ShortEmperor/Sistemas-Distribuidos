/*
 * servidor.c - calculadora distribuida
 * El mismo ejecutable sirve para las 4 computadoras, pero cada una registra
 * en rpcbind SOLO el programa de su operacion:
 *
 *   ./servidor suma | resta | multiplicacion | division
 *
 * Las funciones *_prog_1 (despachadores) las genera: rpcgen -m operaciones.x
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/pmap_clnt.h>
#include "operaciones.h"

static char host[256];

static void registro(const char *op, numeros *n, float r)
{
    printf("[%s] %s(%f, %f) = %f\n", host, op, n->a, n->b, r);
    fflush(stdout);
}

float *suma_1_svc(numeros *argp, struct svc_req *rqstp)
{
    static float resultado;
    resultado = argp->a + argp->b;
    registro("suma", argp, resultado);
    return &resultado;
}

float *resta_1_svc(numeros *argp, struct svc_req *rqstp)
{
    static float resultado;
    resultado = argp->a - argp->b;
    registro("resta", argp, resultado);
    return &resultado;
}

float *multiplicacion_1_svc(numeros *argp, struct svc_req *rqstp)
{
    static float resultado;
    resultado = argp->a * argp->b;
    registro("multiplicacion", argp, resultado);
    return &resultado;
}

float *division_1_svc(numeros *argp, struct svc_req *rqstp)
{
    static float resultado;
    if (argp->b == 0) {
        resultado = 0;   /* el cliente avisa que no se puede dividir entre 0 */
    } else {
        resultado = argp->a / argp->b;
    }
    registro("division", argp, resultado);
    return &resultado;
}

/* funciones generadas por rpcgen -m */
extern void suma_prog_1(struct svc_req *, SVCXPRT *);
extern void resta_prog_1(struct svc_req *, SVCXPRT *);
extern void multiplicacion_prog_1(struct svc_req *, SVCXPRT *);
extern void division_prog_1(struct svc_req *, SVCXPRT *);

int main(int argc, char *argv[])
{
    SVCXPRT *transp;
    unsigned long prog;
    void (*despachador)(struct svc_req *, SVCXPRT *);

    if (argc < 2) {
        printf("Uso: %s suma|resta|multiplicacion|division\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "suma") == 0) {
        prog = SUMA_PROG; despachador = suma_prog_1;
    } else if (strcmp(argv[1], "resta") == 0) {
        prog = RESTA_PROG; despachador = resta_prog_1;
    } else if (strcmp(argv[1], "multiplicacion") == 0) {
        prog = MULTIPLICACION_PROG; despachador = multiplicacion_prog_1;
    } else if (strcmp(argv[1], "division") == 0) {
        prog = DIVISION_PROG; despachador = division_prog_1;
    } else {
        printf("Operacion desconocida: %s\n", argv[1]);
        return 1;
    }

    gethostname(host, sizeof(host));

    /* todas las versiones son 1 */
    pmap_unset(prog, 1);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == NULL || !svc_register(transp, prog, 1, despachador, IPPROTO_UDP)) {
        fprintf(stderr, "No se pudo registrar %s (udp)\n", argv[1]);
        return 1;
    }
    transp = svctcp_create(RPC_ANYSOCK, 0, 0);
    if (transp == NULL || !svc_register(transp, prog, 1, despachador, IPPROTO_TCP)) {
        fprintf(stderr, "No se pudo registrar %s (tcp)\n", argv[1]);
        return 1;
    }

    printf("[%s] Servidor de %s listo (programa 0x%lx)\n", host, argv[1], prog);
    fflush(stdout);

    svc_run();
    fprintf(stderr, "svc_run termino\n");
    return 1;
}
