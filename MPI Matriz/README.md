# Multiplicación de matrices con MPI

Práctica de MPI en C (OpenMPI): multiplicar `C = A × B` repartiendo las filas entre
**4 computadoras**. Llega al tamaño de la práctica: **10,000 × 10,000 en ~143 s** en Docker, e imprime las
matrices A, B y C completas. A y B son de puros 1, así que todos los valores de C son `N`.

## Cómo funciona

Todos los procesos ejecutan el mismo programa; el proceso 0 (maestro) además genera los datos
y junta el resultado.

1. El proceso 0 llena `A` y `B` con **puros 1**. Así el resultado es predecible: cada
   `C[i][j]` es la suma de `N` productos `1 × 1`, o sea **todos los valores de C son `N`**.
2. **`MPI_Bcast`**: `B` completa se manda a todos los procesos.
3. **`MPI_Scatterv`**: a cada proceso le llegan **solo sus filas de A**. Si `N` no es
   divisible entre el número de procesos, los primeros reciben una fila extra (por eso
   `Scatterv` y no `Scatter`).
4. Cada proceso calcula sus filas de `C` (`C_local = A_local × B`) y mide cuánto tardó.
5. **`MPI_Gatherv`**: el proceso 0 junta las filas de todos en `C`.
6. **`MPI_Gather`**: el proceso 0 junta el tiempo de cálculo y el nombre de máquina de cada
   proceso, y reporta qué hizo cada uno.
7. Con `--verificar`, el proceso 0 recalcula todo solo y compara.
8. El proceso 0 **imprime las matrices A, B y C completas** (para cualquier `N`) y al final el
   resumen de tiempos.

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
| `int` | Con A y B de puros 1, cada celda de C vale `N` (10,000 como máximo en la práctica): cabe de sobra en `int` |
| A y B de **puros 1** | Resultado predecible (C = puros `N`), fácil de revisar a simple vista o con un script; el proceso 0 lo comprueba en cada ejecución |
| Se valida `malloc` | Con `N` grande puede faltar memoria; se aborta con mensaje en lugar de fallar después |
| **Solo el proceso 0 imprime** (los demás le mandan su tiempo y host con `MPI_Gather`) | `mpirun` reenvía la salida de cada nodo cuando puede: si cada proceso imprimiera, sus líneas podrían quedar **en medio de las matrices** (se probó: aparecían después de C aun con `MPI_Barrier`) |
| Las matrices se imprimen **después** de medir el tiempo | Imprimir 1 GB de texto no es parte del cálculo distribuido |
| El resumen de tiempos va **al final** | Con matrices grandes, es lo único que se ve sin desplazarse |
| Ancho de columna según el número más grande | Las columnas quedan alineadas aunque C tenga números de más cifras que A y B |
| Cada fila se arma en un buffer y se escribe con `fwrite` | Un `printf` por número es mucho más lento con 300 millones de números |

**Memoria con N = 10,000** (`int` = 4 bytes → 400 MB por matriz)

| Dónde | Qué guarda | Total |
|-------|-----------|-------|
| proceso 0 | A + B + C (+ sus filas) | ~1.4 GB |
| cada otro proceso | B + sus filas de A y de C | ~600 MB |

**Docker**

- 4 contenedores (`nodo1` … `nodo4`) con la misma imagen: `debian:bookworm-slim` + OpenMPI +
  `gcc` + servidor ssh.
- **Solo `nodo1` construye la imagen** (`build` + `pull_policy: build`); `nodo2`…`nodo4` usan
  esa misma imagen (`pull_policy: never`). Así los 4 tienen exactamente la misma llave ssh.
  Si cada nodo construyera su propia imagen, cada uno podría generar una llave distinta y
  `mpirun` no podría entrar a los demás nodos.
- El programa se compila con `mpicc` al construir la imagen, así queda **en la misma ruta en
  todos los nodos** (`/home/mpi/matriz_mpi`), como pide MPI.
- **ssh sin contraseña**: la llave se genera al construir la imagen y la comparten todos los
  nodos.
- Usuario **`mpi`**: `mpirun` no deja correr como `root`.
- `/etc/openmpi/openmpi-mca-params.conf`: MPI usa solo la red del contenedor (`eth0`).
- `shm_size: 1g`: memoria compartida suficiente para OpenMPI.

## Código explicado

Las matrices se guardan en **arreglos de una dimensión**: el elemento `(i, j)` de una matriz
`N × N` está en la posición `i*N + j`. Así cada matriz es un solo bloque de memoria, que es lo
que necesitan las funciones de MPI.

### `matriz_mpi.c`

**`imprimir(nombre, M, N)`**: imprime la matriz completa.

1. Busca el número más grande y cuenta sus cifras → `ancho` de columna.
2. Reserva un buffer para una fila: `N × (ancho + 1)` caracteres + salto de línea.
3. Por cada fila: `sprintf(p, "%*d ", ancho, valor)` para cada número (el `*` toma el ancho
   de la variable), y `fwrite` de la fila completa.
4. `fflush(stdout)` para que salga todo antes de seguir.

**`multiplicar(A, B, C, filas, N)`**: `C[filas × N] = A[filas × N] · B[N × N]`.

```c
memset(C, 0, filas * N * sizeof(int));
for (i = 0; i < filas; i++)
    for (k = 0; k < N; k++) {
        int a = A[i*N + k];                  // un elemento de A
        for (j = 0; j < N; j++)
            C[i*N + j] += a * B[k*N + j];    // lo multiplica por toda la fila k de B
    }
```

Orden **i-k-j** en lugar del clásico i-j-k: el ciclo de adentro recorre `B` y `C` por filas
(posiciones seguidas en memoria), que aprovecha la caché del procesador. Con matrices grandes
es varias veces más rápido.

**`main`**, paso por paso (lo ejecutan **todos** los procesos; `rank` decide qué hace cada uno):

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `MPI_Init`, `MPI_Comm_rank`, `MPI_Comm_size`, `MPI_Get_processor_name` | Inicia MPI; cada proceso sabe su número (`rank`), cuántos son (`size`) y en qué máquina está |
| 2 | ciclo sobre `argv` | `N` (por defecto 1000), `--verificar` y `--no-imprimir`. Si `N` no es válido, el proceso 0 muestra el uso y todos terminan |
| 3 | `filas[p] = N / size + (p < N % size ? 1 : 0)` | Filas de cada proceso; los primeros `N % size` reciben una extra |
| 4 | `inicio[p]`, `conteos[p] = filas[p] * N`, `desplazamientos[p] = inicio[p] * N` | Dónde empieza cada bloque y cuántos enteros lleva: es lo que piden `Scatterv`/`Gatherv` |
| 5 | `malloc` de `B`, `A_local`, `C_local` | **Todos** necesitan B completa y espacio para sus filas. Si falta memoria: `MPI_Abort` (termina a todos) |
| 6 | `if (rank == 0)`: `malloc` de `A` y `C`, `A[i] = 1; B[i] = 1;` | Solo el proceso 0 tiene A y C completas. Llena A y B con puros 1 |
| 7 | `MPI_Barrier` + `t0 = MPI_Wtime()` | Espera a que todos estén listos y empieza a medir |
| 8 | `MPI_Bcast(B, N*N, MPI_INT, 0, MPI_COMM_WORLD)` | El proceso 0 manda B a todos; en los demás se llena su `B` |
| 9 | `MPI_Scatterv(A, conteos, desplazamientos, MPI_INT, A_local, conteos[rank], MPI_INT, 0, ...)` | El proceso 0 corta A en bloques de filas y le manda a cada proceso el suyo (a sí mismo también) |
| 10 | `multiplicar(A_local, B, C_local, filas[rank], N)` | Cada proceso calcula sus filas de C, midiendo el tiempo |
| 11 | `MPI_Gatherv(C_local, conteos[rank], MPI_INT, C, conteos, desplazamientos, MPI_INT, 0, ...)` | Lo contrario de `Scatterv`: el proceso 0 recibe las filas de todos y las acomoda en `C`. Aquí termina la medición (`t_total`) |
| 12 | `MPI_Gather(&t_calc, ...)` y `MPI_Gather(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, ...)` | El proceso 0 recibe el tiempo de cálculo y el nombre de máquina de cada proceso |
| 13 | `if (rank == 0)`: ciclo `printf("Proceso %d en %s: filas ...")` | Reporta, en orden, qué filas hizo cada proceso, en qué máquina y cuánto tardó |
| 14 | `if (verificar)` + ciclo `if (C[i] != N) distintos++` | Con `--verificar` recalcula `A · B` completo en un solo proceso y compara con `memcmp`. Siempre cuenta cuántos valores de C no son `N` |
| 15 | `imprimir("A", ...)`, `imprimir("B", ...)`, `imprimir("C", ...)` | Las 3 matrices completas (se salta con `--no-imprimir`) |
| 16 | `printf("Tiempo total ...")` | Resumen al final: tiempo distribuido, `Todos los valores de C son N: CORRECTO` (o cuántos no lo son) y, con `--verificar`, `CORRECTO`/`ERROR` contra la versión secuencial y speedup |
| 17 | `free` + `MPI_Finalize` | Libera memoria y cierra MPI |

**Por qué `Scatterv`/`Gatherv` y no `Scatter`/`Gather`**: las versiones sin `v` exigen que
todos los bloques sean del mismo tamaño; con `v` cada proceso puede recibir un número distinto
de filas (N no divisible entre los procesos, o más procesos que filas: `conteos[p] = 0`).

### `hosts`

```
nodo1 slots=1
nodo2 slots=1
nodo3 slots=1
nodo4 slots=1
```

Un proceso por máquina: el proceso 0 queda en `nodo1`, el 1 en `nodo2`, etc.

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `FROM debian:bookworm-slim` | Sistema base |
| `apt-get install openmpi-bin libopenmpi-dev gcc ... openssh-server openssh-client ...` | OpenMPI (con `mpicc` y los encabezados), compilador y ssh |
| `useradd -m -s /bin/bash mpi` | Usuario normal: `mpirun` se niega a correr como `root` |
| `ssh-keygen -A` + `StrictHostKeyChecking no` | Llaves del servidor ssh; no preguntar al conectarse por primera vez a otro nodo |
| `ssh-keygen -t ed25519 ...` + `authorized_keys` (como `mpi`) | Llave del usuario autorizada en la misma imagen → todos los nodos confían entre sí |
| `btl_tcp_if_include = eth0`, `oob_tcp_if_include = eth0` | MPI usa solo la red del contenedor (para datos y para control) |
| `btl_vader_single_copy_mechanism = none` | Evita un aviso de OpenMPI dentro de contenedores |
| `COPY matriz_mpi.c hosts ./` + `mpicc -O2 -o matriz_mpi matriz_mpi.c` | Compila el programa en `/home/mpi` (misma ruta en todos los nodos). `mpicc` es `gcc` con las rutas y bibliotecas de MPI; `-O2` optimiza la multiplicación |
| `CMD ["/usr/sbin/sshd", "-D", "-e"]` | El contenedor solo corre el servidor ssh esperando a que `mpirun` entre |

### `docker-compose.yml`

- `nodo1`: construye la imagen (`build: .`, `pull_policy: build`) como `mpi-matriz-nodo:latest`.
- `nodo2` … `nodo4`: usan esa misma imagen (`pull_policy: never`) y arrancan después de `nodo1`.
- `hostname: nodoX`: los nombres del archivo `hosts`.
- `shm_size: 1g`: memoria compartida para OpenMPI.

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

Uso: `mpirun -np <procesos> --hostfile hosts ./matriz_mpi [N] [--verificar] [--no-imprimir]`

- `N`: tamaño de la matriz (por defecto 1000).
- Siempre se imprimen **A, B y C completas**, sea cual sea `N`.
- `--verificar`: el proceso 0 recalcula todo solo, compara e imprime el speedup
  (no usar con `N` muy grande: tarda lo mismo que una sola máquina).
- `--no-imprimir`: no imprime las matrices (para medir tiempos sin generar el texto).

El orden de la salida es siempre: qué hizo cada proceso → matriz A → matriz B → matriz C →
resumen de tiempos.

**Tamaño de la salida**: cada número ocupa sus cifras + un espacio.

| N | Tamaño de la salida | Tiempo en imprimir |
|---|--------------------|--------------------|
| 2,000 | 35 MB | ~2 s |
| 10,000 | **954 MB** | ~35 s |

Con `N` grande **no conviene imprimir en la terminal** (tardaría muchísimo); mejor mandarlo a un
archivo dentro del contenedor y copiarlo:

```bash
docker exec -u mpi mpi-matriz-nodo1 sh -c "mpirun -np 4 --hostfile hosts ./matriz_mpi 10000 > salida.txt"
docker cp mpi-matriz-nodo1:/home/mpi/salida.txt .
```

> En PowerShell 5.1 evitar `docker exec ... > salida.txt` directo: `>` guarda el archivo en
> UTF-16 y lo hace el doble de grande. Con `sh -c "... > salida.txt"` la redirección ocurre
> dentro del contenedor.

**Matriz pequeña**

```bash
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 4 --verificar
```
```
Multiplicando matrices de 4x4 con 4 procesos
Proceso 0 en nodo1: filas 0 a 0 (0.000 s de calculo)
Proceso 1 en nodo2: filas 1 a 1 (0.000 s de calculo)
Proceso 2 en nodo3: filas 2 a 2 (0.000 s de calculo)
Proceso 3 en nodo4: filas 3 a 3 (0.000 s de calculo)
Matriz A (4x4):
1 1 1 1
1 1 1 1
1 1 1 1
1 1 1 1
Matriz B (4x4):
1 1 1 1
1 1 1 1
1 1 1 1
1 1 1 1
Matriz C (4x4):
4 4 4 4
4 4 4 4
4 4 4 4
4 4 4 4
Tiempo total (envio + calculo + recoleccion): 0.002 s
Todos los valores de C son 4 (= N): CORRECTO
Verificacion contra version secuencial: CORRECTO
Tiempo secuencial: 0.000 s  ->  speedup: 0.00x
```

**Speedup (N = 1500, sin imprimir las matrices)**

```bash
docker exec -it -u mpi mpi-matriz-nodo1 mpirun -np 4 --hostfile hosts ./matriz_mpi 1500 --verificar --no-imprimir
```
```
Multiplicando matrices de 1500x1500 con 4 procesos
Proceso 0 en nodo1: filas 0 a 374 (0.681 s de calculo)
Proceso 1 en nodo2: filas 375 a 749 (0.657 s de calculo)
Proceso 2 en nodo3: filas 750 a 1124 (0.636 s de calculo)
Proceso 3 en nodo4: filas 1125 a 1499 (0.676 s de calculo)
Tiempo total (envio + calculo + recoleccion): 0.752 s
Todos los valores de C son 1500 (= N): CORRECTO
Verificacion contra version secuencial: CORRECTO
Tiempo secuencial: 2.291 s  ->  speedup: 3.05x
```

**Tamaño de la práctica (N = 10,000)** — unos 3 minutos (2:23 de cálculo + ~35 s imprimiendo)

```bash
docker exec -u mpi mpi-matriz-nodo1 sh -c "mpirun -np 4 --hostfile hosts ./matriz_mpi 10000 > salida.txt"
docker exec -u mpi mpi-matriz-nodo1 sh -c "head -5 salida.txt; tail -2 salida.txt"
```
```
Multiplicando matrices de 10000x10000 con 4 procesos
Proceso 0 en nodo1: filas 0 a 2499 (139.070 s de calculo)
Proceso 1 en nodo2: filas 2500 a 4999 (139.237 s de calculo)
Proceso 2 en nodo3: filas 5000 a 7499 (139.685 s de calculo)
Proceso 3 en nodo4: filas 7500 a 9999 (141.066 s de calculo)
Tiempo total (envio + calculo + recoleccion): 143.496 s
Todos los valores de C son 10000 (= N): CORRECTO
```

El archivo mide 954 MB y tiene 30,010 líneas: 5 de encabezado, 3 matrices de
10,000 filas × 10,000 columnas (más su título) y las 2 líneas del resumen.

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
