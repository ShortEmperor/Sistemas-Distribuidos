# Multiplicación de matrices con MPI

Práctica de MPI en C (OpenMPI): multiplicar `C = A × B` repartiendo las filas entre
**4 computadoras**. Llega al tamaño de la práctica: **10,000 × 10,000 en 153 s** en Docker.

## Cómo funciona

Todos los procesos ejecutan el mismo programa; el proceso 0 (maestro) además genera los datos
y junta el resultado.

1. El proceso 0 genera `A` y `B` (enteros del 0 al 9, semilla fija).
2. **`MPI_Bcast`**: `B` completa se manda a todos los procesos.
3. **`MPI_Scatterv`**: a cada proceso le llegan **solo sus filas de A**. Si `N` no es
   divisible entre el número de procesos, los primeros reciben una fila extra (por eso
   `Scatterv` y no `Scatter`).
4. Cada proceso calcula sus filas de `C` (`C_local = A_local × B`) y mide cuánto tardó.
5. **`MPI_Gatherv`**: el proceso 0 junta las filas de todos en `C`.
6. Con `--verificar`, el proceso 0 recalcula todo solo, compara y muestra el speedup.

```
 proceso 0 (nodo1)                     procesos 1..3 (nodo2..nodo4)
 ┌─────────────────┐
 │ genera A, B     │
 │                 │ ── MPI_Bcast(B) ─────────────────► todos reciben B
 │                 │ ── MPI_Scatterv(A) ──────────────► cada uno sus filas de A
 │ C_local = A·B   │                                    C_local = A_local · B
 │                 │ ◄─ MPI_Gatherv(C) ─────────────── cada uno manda sus filas de C
 │ C completa      │
 └─────────────────┘
```

## Diseño

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `matriz_mpi.c` | Programa MPI |
| `hosts` | Hostfile: `nodo1` … `nodo4`, 1 proceso por nodo |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Decisiones**

| Decisión | Por qué |
|----------|---------|
| Operaciones colectivas (`Bcast`, `Scatterv`, `Gatherv`) en lugar de `Send`/`Recv` a mano | Menos código y MPI las optimiza |
| Reparto por **bloques de filas** | Cada proceso solo necesita sus filas de A (y B completa); sus filas de C no dependen de nadie más |
| `Scatterv` / `Gatherv` con `conteos` y `desplazamientos` | Funciona aunque `N` no sea divisible entre los procesos, y con más procesos que filas |
| Matrices en arreglos 1D (`M[i*N + j]`) con `malloc` | Memoria contigua, que es lo que piden las funciones de MPI |
| Multiplicación en orden `i-k-j` | Recorre `B` y `C` por filas (mejor uso de la caché) |
| `int` | Con valores del 0 al 9, el máximo de una celda es 81 × 10000 = 810,000: cabe en `int` |
| Se valida `malloc` | Con `N` grande puede faltar memoria; se aborta con mensaje en lugar de fallar después |

**Memoria con N = 10,000** (`int` = 4 bytes → 400 MB por matriz)

| Dónde | Qué guarda | Total |
|-------|-----------|-------|
| proceso 0 | A + B + C (+ sus filas) | ~1.4 GB |
| cada otro proceso | B + sus filas de A y de C | ~600 MB |

**Docker**

- 4 contenedores (`nodo1` … `nodo4`) con la misma imagen: `debian:bookworm-slim` + OpenMPI +
  `gcc` + servidor ssh.
- El programa se compila con `mpicc` al construir la imagen, así queda **en la misma ruta en
  todos los nodos** (`/home/mpi/matriz_mpi`), como pide MPI.
- **ssh sin contraseña**: la llave se genera al construir la imagen y la comparten todos los
  nodos.
- Usuario **`mpi`**: `mpirun` no deja correr como `root`.
- `/etc/openmpi/openmpi-mca-params.conf`: MPI usa solo la red del contenedor (`eth0`).
- `shm_size: 1g`: memoria compartida suficiente para OpenMPI.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

Si se cambia `matriz_mpi.c` hay que reconstruir: `docker compose up -d --build`.

| Contenedor | Rol |
|------------|-----|
| `mpi-matriz-nodo1` | nodo 1 (proceso 0, desde aquí se lanza `mpirun`) |
| `mpi-matriz-nodo2` … `mpi-matriz-nodo4` | nodos 2 a 4 |

Los comandos siempre se ejecutan como usuario `mpi` (`-u mpi`).

## Pruebas

Uso: `mpirun -np <procesos> --hostfile hosts ./matriz_mpi [N] [--verificar]`

- `N`: tamaño de la matriz (por defecto 1000).
- `--verificar`: el proceso 0 recalcula todo solo, compara e imprime el speedup
  (no usar con `N` muy grande: tarda lo mismo que una sola máquina).
- Con `N <= 10` se imprimen A, B y C.

**Matriz pequeña (se imprime todo)**

```bash
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 4 --verificar
```
```
Proceso 0 en nodo1: filas 0 a 0 (0.000 s de calculo)
Proceso 1 en nodo2: filas 1 a 1 (0.000 s de calculo)
Proceso 2 en nodo3: filas 2 a 2 (0.000 s de calculo)
Proceso 3 en nodo4: filas 3 a 3 (0.000 s de calculo)
...
Matriz C:
  28   31   62   15
 122  104  106   71
 161  125  116  100
  62   55  112   42
Verificacion contra version secuencial: CORRECTO
```

**Speedup (N = 1500)**

```bash
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 1500 --verificar
```
```
Tiempo total (envio + calculo + recoleccion): 0.752 s
Verificacion contra version secuencial: CORRECTO
Tiempo secuencial: 2.252 s  ->  speedup: 3.00x
```

**Tamaño de la práctica (N = 10,000)** — tarda unos 2-3 minutos

```bash
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 10000
```
```
Proceso 0 en nodo1: filas 0 a 2499 (149.464 s de calculo)
Proceso 1 en nodo2: filas 2500 a 4999 (150.335 s de calculo)
Proceso 2 en nodo3: filas 5000 a 7499 (147.673 s de calculo)
Proceso 3 en nodo4: filas 7500 a 9999 (148.421 s de calculo)
Tiempo total (envio + calculo + recoleccion): 153.034 s
```

Mientras corre se puede ver que los 4 nodos trabajan al mismo tiempo: `docker stats`.

**Otros casos**

```bash
# N no divisible entre los procesos (7 filas entre 3)
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 3 --hostfile hosts ./matriz_mpi 7 --verificar

# más procesos que filas: los que sobran avisan "sin filas"
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 2 --verificar

# comprobar que son 4 máquinas distintas
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts hostname
```

**Entrar a un nodo**

```bash
docker exec -it -u mpi mpi-matriz-nodo1 bash
```

## En máquinas reales (sin Docker)

Preparar las máquinas como en `../MPI HolaMundo/Librerias` (ssh sin contraseña, `/etc/hosts`,
firewall). Después:

```bash
sudo dnf install openmpi openmpi-devel -y
export PATH="$HOME/.local/bin:$HOME/bin:/usr/lib64/openmpi/bin:$PATH"
mpicc -O2 -o matriz_mpi matriz_mpi.c
# copiar matriz_mpi a la misma ruta en todas las máquinas y ajustar "hosts"
mpirun -np 4 --hostfile hosts ./matriz_mpi 10000
```
