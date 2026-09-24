/*
 * cliente.c - calculadora distribuida
 * Cada operacion esta en una computadora distinta:
 *
 *   ./cliente <host_suma> <host_resta> <host_multiplicacion> <host_division>
 */

#include <stdio.h>
#include <stdlib.h>
#include "operaciones.h"

int main(int argc, char *argv[])
{
    CLIENT *clnt_suma, *clnt_resta, *clnt_mult, *clnt_div;
    numeros nums;
    float *resultado;

    if (argc < 5) {
        printf("Uso: %s host_suma host_resta host_multiplicacion host_division\n", argv[0]);
        return 1;
    }

    /* una conexion a cada computadora */
    clnt_suma = clnt_create(argv[1], SUMA_PROG, SUMA_VERS, "udp");
    if (clnt_suma == NULL) { clnt_pcreateerror(argv[1]); return 1; }

    clnt_resta = clnt_create(argv[2], RESTA_PROG, RESTA_VERS, "udp");
    if (clnt_resta == NULL) { clnt_pcreateerror(argv[2]); return 1; }

    clnt_mult = clnt_create(argv[3], MULTIPLICACION_PROG, MULTIPLICACION_VERS, "udp");
    if (clnt_mult == NULL) { clnt_pcreateerror(argv[3]); return 1; }

    clnt_div = clnt_create(argv[4], DIVISION_PROG, DIVISION_VERS, "udp");
    if (clnt_div == NULL) { clnt_pcreateerror(argv[4]); return 1; }

    printf("Ingresa el primer numero: ");
    scanf("%f", &nums.a);

    printf("Ingresa el segundo numero: ");
    scanf("%f", &nums.b);

    resultado = suma_1(&nums, clnt_suma);
    if (resultado == NULL) { clnt_perror(clnt_suma, "Error en Suma"); return 1; }
    printf("Suma (%s): %f\n", argv[1], *resultado);

    resultado = resta_1(&nums, clnt_resta);
    if (resultado == NULL) { clnt_perror(clnt_resta, "Error en Resta"); return 1; }
    printf("Resta (%s): %f\n", argv[2], *resultado);

    resultado = multiplicacion_1(&nums, clnt_mult);
    if (resultado == NULL) { clnt_perror(clnt_mult, "Error en Multiplicacion"); return 1; }
    printf("Multiplicacion (%s): %f\n", argv[3], *resultado);

    resultado = division_1(&nums, clnt_div);
    if (resultado == NULL) { clnt_perror(clnt_div, "Error en Division"); return 1; }
    if (nums.b == 0)
        printf("Division (%s): no se puede dividir entre 0\n", argv[4]);
    else
        printf("Division (%s): %f\n", argv[4], *resultado);

    clnt_destroy(clnt_suma);
    clnt_destroy(clnt_resta);
    clnt_destroy(clnt_mult);
    clnt_destroy(clnt_div);
    return 0;
}
