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
    if (resultado == NULL) {
        clnt_perror(clnt, "Error en Suma");
        return 1;
    }
    printf("Suma: %f\n", *resultado);

    resultado = resta_1(&nums, clnt);
    if (resultado == NULL) {
        clnt_perror(clnt, "Error en Resta");
        return 1;
    }
    printf("Resta: %f\n", *resultado);

    resultado = multiplicacion_1(&nums, clnt);
    if (resultado == NULL) {
        clnt_perror(clnt, "Error en Multiplicacion");
        return 1;
    }
    printf("Multiplicacion: %f\n", *resultado);

    resultado = division_1(&nums, clnt);
    if (resultado == NULL) {
        clnt_perror(clnt, "Error en Division");
        return 1;
    }
    if (nums.b == 0) {
        printf("Division: no se puede dividir entre 0\n");
    } else {
        printf("Division: %f\n", *resultado);
    }

    clnt_destroy(clnt);
    return 0;
}
