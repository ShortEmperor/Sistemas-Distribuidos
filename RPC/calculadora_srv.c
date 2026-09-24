#include "calculadora.h"

float *suma_1_svc(numeros *argp, struct svc_req *rqstp) {
    static float resultado;
    resultado = argp->a + argp->b;
    return &resultado;
}

float *resta_1_svc(numeros *argp, struct svc_req *rqstp) {
    static float resultado;
    resultado = argp->a - argp->b;
    return &resultado;
}

float *multiplicacion_1_svc(numeros *argp, struct svc_req *rqstp) {
    static float resultado;
    resultado = argp->a * argp->b;
    return &resultado;
}

float *division_1_svc(numeros *argp, struct svc_req *rqstp) {
    static float resultado;

    if (argp->b == 0) {
        resultado = 0;
    } else {
        resultado = argp->a / argp->b;
    }

    return &resultado;
}
