struct numeros {
    float a;
    float b;
};

program OPERACIONES_PROG {
    version OPERACIONES_VERS {
        float SUMA(numeros) = 1;
        float RESTA(numeros) = 2;
        float MULTIPLICACION(numeros) = 3;
        float DIVISION(numeros) = 4;
    } = 1;
} = 0x20000001;
