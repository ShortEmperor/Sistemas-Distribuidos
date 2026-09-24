#include <stdio.h>
#include "calculadora.h"

int main(int argc, char *argv[]) {

    CLIENT *clnt;
    numeros nums;
    float *resultado;
    char *host;

    if (argc < 2) {
        printf("Uso: %s servidor\n", argv[0]);
        return 1;
    }

    host = argv[1];

    clnt = clnt_create(host, OPERACIONES_PROG, OPERACIONES_VERS, "udp");

    if (clnt == NULL) {
        clnt_pcreateerror(host);
        return 1;
    }

    printf("Ingresa el primer numero: ");
    scanf("%f", &nums.a);

    printf("Ingresa el segundo numero: ");
    scanf("%f", &nums.b);

    resultado = suma_1(&nums, clnt);
    printf("Suma: %f\n", *resultado);

    resultado = resta_1(&nums, clnt);
    printf("Resta: %f\n", *resultado);

    resultado = multiplicacion_1(&nums, clnt);
    printf("Multiplicacion: %f\n", *resultado);

    resultado = division_1(&nums, clnt);
    printf("Division: %f\n", *resultado);

    clnt_destroy(clnt);
    return 0;
}
