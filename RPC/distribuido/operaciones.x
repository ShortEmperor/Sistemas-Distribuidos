/*
 * Calculadora distribuida: cada operacion es un programa RPC distinto,
 * asi cada computadora registra (y atiende) solo la suya.
 */
struct numeros {
    float a;
    float b;
};

program SUMA_PROG {
    version SUMA_VERS {
        float SUMA(numeros) = 1;
    } = 1;
} = 0x20000011;

program RESTA_PROG {
    version RESTA_VERS {
        float RESTA(numeros) = 1;
    } = 1;
} = 0x20000012;

program MULTIPLICACION_PROG {
    version MULTIPLICACION_VERS {
        float MULTIPLICACION(numeros) = 1;
    } = 1;
} = 0x20000013;

program DIVISION_PROG {
    version DIVISION_VERS {
        float DIVISION(numeros) = 1;
    } = 1;
} = 0x20000014;
