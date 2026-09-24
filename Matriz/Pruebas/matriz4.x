struct matrices4{
	int n;
	int filas_inicio;
	int filas_fin;
	int A[100];
	int B[100];
};

struct resultado4{
	int n;
	int fila_inicio;
	int fila_fin;
	int C[100];
};

program MATRIZ_PROG1{
	version MATRIZ_VERS {
		resultado4 MULTIPLICAR4(matrices4) = 1;
} = 1;
} = 0x20000004;
