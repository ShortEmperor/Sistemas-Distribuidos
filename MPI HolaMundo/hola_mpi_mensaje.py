# hola_mpi_mensaje.py
# Igual que hola_mpi.py pero ahora si se manda un mensaje entre procesos:
# el proceso 0 le manda un saludo a cada uno de los demas y ellos le responden.
import socket

from mpi4py import MPI

# Inicializar el comunicador
comm = MPI.COMM_WORLD
# Obtener el rango del proceso actual
rank = comm.Get_rank()
# Obtener el número total de procesos
size = comm.Get_size()
# Nombre de la maquina donde corre este proceso
host = socket.gethostname()

if size < 2:
    print("Se necesitan al menos 2 procesos: mpirun -np 2 ...")
elif rank == 0:
    for destino in range(1, size):
        comm.send(f"Hola Mundo desde el proceso 0 en {host}", dest=destino, tag=1)
    for origen in range(1, size):
        respuesta = comm.recv(source=origen, tag=2)
        print(f"Proceso 0 en {host} recibio: '{respuesta}'", flush=True)
else:
    mensaje = comm.recv(source=0, tag=1)
    print(f"Proceso {rank} en {host} recibio: '{mensaje}'", flush=True)
    comm.send(f"Hola de vuelta desde el proceso {rank} en {host}", dest=0, tag=2)
