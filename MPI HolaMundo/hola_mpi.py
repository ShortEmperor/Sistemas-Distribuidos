# hola_mpi.py
from mpi4py import MPI

# Inicializar el comunicador
comm = MPI.COMM_WORLD
# Obtener el rango del proceso actual
rank = comm.Get_rank()
# Obtener el número total de procesos
size = comm.Get_size()

print(f"Hola Mundo desde el proceso {rank} de {size}")
