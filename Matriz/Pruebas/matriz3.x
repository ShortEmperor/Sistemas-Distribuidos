struct matrices3 {
	int n;
	int fila_inicio;
	int fila_fin;
	int A[100];
	int B[100];
};

struct resultado3 {
	int n;
	int fila_inicio;
	int fila_fin;
	int C[100];
};

program MATRIZ_PROG3{
	version MATRIZ_VERS {
		resultado3 MULTIPLICAR3(matrices3) = 1;
	} = 1;	
} = 0x20000003;
