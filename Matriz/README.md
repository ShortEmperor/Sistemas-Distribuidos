# Multiplicación de matrices con RPC (versión de 2 servidores)

Práctica de RPC en C: el cliente reparte la multiplicación `C = A × B` entre **2 servidores**;
cada uno calcula la mitad de las filas de `C` y el cliente junta el resultado.

> La práctica pedía matrices de **10,000 × 10,000 entre 4 computadoras**. Esta es la versión
> que se hizo (2 servidores y matrices pequeñas porque la computadora no aguantaba). La versión
> que sí cumple la práctica está en [`distribuido/`](distribuido/).
>
> `readme.txt` es el readme original de la práctica y se deja igual.

## Cómo funciona

1. `servidor1` y `servidor2` son **programas RPC distintos** (`MATRIZ_PROG1 = 0x20000001` y
   `MATRIZ_PROG2 = 0x20000002`). Por tener números distintos pueden correr los dos en la misma
   máquina, que es como se probó originalmente (`localhost`).
2. El cliente (`cliente1.c`) llena `A` y `B` (todos los valores en 1) y divide las filas en 2:
   - servidor 1: filas `0` a `N/2 - 1`
   - servidor 2: filas `N/2` a `N - 1`
3. A cada servidor le manda `n`, `fila_inicio`, `fila_fin` y **las matrices A y B completas**.
4. Cada servidor calcula sus filas de `C` y las regresa en un arreglo del tamaño completo.
5. El cliente copia las filas de cada respuesta en la matriz `C` final y la imprime.
   Como A y B son puros 1, **C debe salir con puros `N`**.

```
 ┌───────────────────┐  A, B completas, filas 0..N/2-1   ┌─────────────────────┐
 │ cliente1          │ ────────────────────────────────► │ servidor1 (PROG1)   │
 │                   │ ◄──────────────────────────────── │ multiplicar1_1_svc  │
 │  C = C1 + C2      │         C (filas 0..N/2-1)        └─────────────────────┘
 │                   │  A, B completas, filas N/2..N-1   ┌─────────────────────┐
 │                   │ ────────────────────────────────► │ servidor2 (PROG2)   │
 └───────────────────┘ ◄──────────────────────────────── │ multiplicar2_1_svc  │
                              C (filas N/2..N-1)         └─────────────────────┘
```

Las llamadas son **una después de otra**: mientras un servidor calcula, el otro espera.

## Diseño

**Estructuras (de `matriz.h` / `matriz2.h`, generados con rpcgen)**

```c
struct matrices1 { int n; int fila_inicio; int fila_fin; int A[100]; int B[100]; };
struct resultado1 { int n; int fila_inicio; int fila_fin; int C[100]; };
// matrices2 / resultado2: iguales, para el servidor 2
```

Los arreglos son de **tamaño fijo**: se mandan siempre completos aunque se use menos, y `N×N`
no puede pasar del tamaño del arreglo.

**Contenido de la carpeta** (todo se dejó como evidencia de los intentos)

| Archivo / carpeta | Qué es |
|-------------------|--------|
| `cliente1.c` | **Cliente que funciona** (2 servidores, `N` por argumento, de 1 a 10) |
| `servidor1.c`, `servidor2.c` | **Servidores que funcionan** |
| `matriz.h`, `matriz_*.c` / `matriz2.h`, `matriz2_*.c` | Generados por rpcgen para PROG1 / PROG2 (arreglos de 100) |
| `matriz.x`, `matriz2.x` | Definiciones RPC; **no coinciden** con los `.h` que se usan (ver errores) |
| `cliente.c` | Intento anterior (puertos fijos 5001/5002 con `clntudp_create`); no compila |
| `clientem.c` | Intento anterior (un solo tipo `matrices`); no compila |
| `servidor.c`, `cliente1.x` | Vacíos |
| `Servidor1/`, `Servidor2/` | Copias para correr cada servidor en una máquina distinta (arreglos de 4) |
| `Pruebas/` | Variante con arreglos de 10000 (`N` de 1 a 100, por defecto 100); `matriz3.x` y `matriz4.x` son borradores para 4 servidores |
| `cliente`, `cliente1`, `*/servidor*` | Binarios viejos (no se usan) |
| `readme.txt` | Readme original |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Docker**

- La imagen compila **2 variantes** usando los `.h`/`.c` generados que ya estaban en el repo:
  - `principal`: archivos de esta carpeta (arreglos de 100 → `N` máximo 10).
  - `pruebas`: carpeta `Pruebas/` (arreglos de 10000 → `N` máximo 100).
- La variable `VARIANTE` elige qué servidores arrancan (por defecto `principal`). Cliente y
  servidores **deben ser de la misma variante** (los tamaños de los arreglos tienen que coincidir).
- Cada servidor arranca `rpcbind` y después su programa. `stdbuf -oL` hace que sus `printf`
  aparezcan en `docker logs`.

## Código explicado

> Para lo básico de RPC (qué hacen `clnt_create`, los stubs, XDR, rpcbind, `svc_register`)
> ver la sección "Código explicado" de [`../RPC/README.md`](../RPC/README.md).

Las matrices se guardan en **arreglos de una dimensión**: el elemento `(i, j)` de una matriz
`N × N` está en la posición `i*N + j`.

### `cliente1.c` — cliente de 2 servidores (el que funciona)

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `#include "matriz.h"` y `#include "matriz2.h"` | Incluye los 2 programas RPC: tipos `matrices1`/`resultado1` (servidor 1) y `matrices2`/`resultado2` (servidor 2) |
| 2 | `#define SERV 2`, `int N = N_DEFECTO` (10) | 2 servidores; `N` es una variable para poder cambiarla con un argumento |
| 3 | `servidores[SERV] = {"localhost", "localhost"}` + `if (argc >= 3)` | Por defecto los 2 servidores en la misma máquina; con argumentos se usan otros hosts |
| 4 | `programas[SERV] = {MATRIZ_PROG1, MATRIZ_PROG2}` | Número de programa de cada servidor |
| 5 | `int C[1024]; memset(C, 0, ...)` | Matriz resultado completa, en el cliente |
| 6 | `elementos = sizeof(m1.A) / sizeof(int)` (y lo mismo con `m2.A` y `C`, se queda el menor); `while ((n_max+1)*(n_max+1) <= elementos) n_max++` | Calcula el `N` máximo **a partir del tamaño real de los arreglos** de los `.h`: con `A[100]` da 10, con `A[10000]` da 100 |
| 7 | `if (argc >= 4) N = atoi(argv[3])` + `if (N < 1 \|\| N > n_max)` | `N` opcional como tercer argumento. Si no cabe en los arreglos se rechaza con un mensaje, en lugar de desbordarlos (lo que colgaba al cliente con `N 32`) |
| 8 | 2 ciclos `m1.A[i*N + j] = 1`, `m1.B[...] = 1` | Llena A y B con puros 1 (la fórmula original quedó comentada) y las imprime |
| 9 | `bloque = N / SERV` | Filas por servidor (con N = 10: 5) |
| 10 | `clnt_create(servidores[s], programas[s], MATRIZ_VERS, "udp")` | Conecta al servidor `s` por UDP |
| 11 | `s == 0`: `m1.fila_inicio = 0; m1.fila_fin = bloque;` `res1 = multiplicar1_1(&m1, clnt)` | Pide al servidor 1 las filas `0` a `bloque - 1`. Imprime esas filas de `res1->C` y las copia en `C` |
| 12 | `s == 1`: `m2.fila_inicio = bloque; m2.fila_fin = N;` `memcpy(m2.A, m1.A, ...)` `res2 = multiplicar2_1(&m2, clnt)` | Como el servidor 2 usa otro tipo (`matrices2`), se copian A y B de `m1` a `m2`. Pide las filas `bloque` a `N - 1` (con N impar le toca una más) y las copia en `C` |
| 13 | `clnt_destroy(clnt)` | Cierra la conexión (dentro del ciclo) |
| 14 | último ciclo | Imprime la matriz `C` completa |

Las llamadas están dentro de un `for`: **primero el servidor 1 y cuando responde el servidor 2**.

### `servidor1.c` / `servidor2.c` — los servidores

```c
resultado1 *multiplicar1_1_svc(matrices1 *m, struct svc_req *req) {
    static resultado1 res;
    n = m->n;
    res.n = n; res.fila_inicio = m->fila_inicio; res.fila_fin = m->fila_fin;
    for (i = m->fila_inicio; i < m->fila_fin; i++)      // solo sus filas
        for (j = 0; j < n; j++) {                       // todas las columnas
            res.C[i*n+j] = 0;
            for (k = 0; k < n; k++)
                res.C[i*n+j] += m->A[i*n+k] * m->B[k*n+j];   // fila i de A · columna j de B
        }
    printf("Servidor 1 calculó filas %d a %d\n", ...);
    return &res;
}
```

- Recibe A y B completas pero calcula solo las filas `[fila_inicio, fila_fin)`.
- Regresa `res` completo (100 enteros) aunque solo haya llenado sus filas; el cliente solo
  lee esas filas.
- `servidor2.c` es idéntico con `matrices2`/`resultado2`/`multiplicar2_1_svc` y el mensaje
  "Servidor 2".

### Archivos generados (`matriz*.h`, `matriz*_clnt.c`, `matriz*_svc.c`, `matriz*_xdr.c`)

- **`matriz.h` / `matriz2.h`**: `struct matrices1 { int n, fila_inicio, fila_fin; int A[100]; int B[100]; }`,
  `struct resultado1 { ...; int C[100]; }`, `#define MATRIZ_PROG1 0x20000001` (y `MATRIZ_PROG2
  0x20000002`), `MATRIZ_VERS 1`, `MULTIPLICAR1 1` y los prototipos.
- **`matriz_xdr.c` / `matriz2_xdr.c`**: `xdr_matrices1` codifica `n`, `fila_inicio`,
  `fila_fin` y después **los 100 elementos de A y los 100 de B** (`xdr_vector` con tamaño
  fijo). Por eso siempre viaja todo el arreglo, aunque `N` sea menor. Tiene una ruta rápida
  (`XDR_INLINE`) que escribe los enteros directo al buffer cuando hay espacio.
- **`matriz_clnt.c` / `matriz2_clnt.c`**: stubs `multiplicar1_1` / `multiplicar2_1` con
  `clnt_call`, timeout de 25 s y resultado `static`.
- **`matriz_svc.c` / `matriz2_svc.c`**: `main` que registra `MATRIZ_PROG1` (o `PROG2`) por UDP
  y TCP y el despachador `matriz_prog1_1` (o `matriz_prog2_1`).

### Intentos anteriores (se dejan como evidencia)

- **`cliente.c`**: `N 2`, se conecta a `127.0.0.1` en los **puertos fijos 5001 y 5002** con
  `clntudp_create(&addr, ...)` (sin pasar por rpcbind) y llama `multiplicar_1`. Es de una
  versión anterior con un solo tipo `matrices`; ya no compila.
- **`clientem.c`**: `N 4`, parecido a `Practica2_matrices/clientem.c` pero con
  `programas[] = {MATRIZ_PROG, MATRIZ_PROG2}`; usa los tipos `matrices`/`resultado`, que ya no
  existen en los `.h`; no compila.
- **`Servidor1/`, `Servidor2/`**: copias de cada servidor para compilarlo en su propia máquina.
  Sus `.x` usan los tipos `matrices`/`resultado` con arreglos de **4** (N máximo 2).
  `Servidor1/servidor.c` y `Servidor2/cliente1.c` están vacíos.
- **`Pruebas/`**: mismos `cliente1.c`, `servidor1.c`, `servidor2.c` pero con `N` por defecto 100
  y arreglos de 10000 (sus `.x` sí coinciden con sus `.h`). `matriz3.x` (+ sus archivos
  generados) es un tercer programa `0x20000003` sin servidor; `matriz4.x` es un borrador de un
  cuarto (`0x20000004`, con el nombre `MATRIZ_PROG1` repetido y el campo `filas_inicio`).
- **`matriz.x` / `matriz2.x`** de esta carpeta: no coinciden con los `.h` que se usan
  (ver errores).

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `apt-get install gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase ...` | Igual que en `RPC/` |
| `COPY *.c *.h ./principal/` y `COPY Pruebas/*.c Pruebas/*.h ./pruebas/` | Dos copias del código, una por variante. **No** se corre rpcgen: se usan los `.h`/`.c` generados que ya estaban en el repo |
| `for v in principal pruebas; do ... gcc ...; done` | Compila en cada variante `servidor1`, `servidor2` y `cliente1` |
| `CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./$VARIANTE/servidor$SERVIDOR"]` | Arranca rpcbind y luego el servidor que indiquen las variables del compose. `stdbuf -oL` vacía `stdout` en cada línea para que los `printf` salgan en `docker logs` |

### `docker-compose.yml`

- `servidor1` y `servidor2`: misma imagen, `SERVIDOR=1` o `2`, y
  `VARIANTE=${VARIANTE:-principal}` (toma la variable de la terminal, o `principal` si no hay).
- `cliente`: `sleep infinity`; dentro están `./principal/cliente1` y `./pruebas/cliente1`.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

| Contenedor | Rol |
|------------|-----|
| `rpc-matriz-servidor1` | servidor 1 (`MATRIZ_PROG1`) |
| `rpc-matriz-servidor2` | servidor 2 (`MATRIZ_PROG2`) |
| `rpc-matriz-cliente` | cliente |

## Pruebas

Uso: `./cliente1 [host_servidor1 host_servidor2 [N]]`

- Sin argumentos: los 2 servidores en `localhost` y el `N` por defecto.
- `N` es opcional y va **después** de los 2 hosts. Tiene que caber en los arreglos de los `.h`:
  de 1 a **10** en `principal` (arreglos de 100) y de 1 a **100** en `pruebas` (arreglos de
  10000). Si no cabe, el cliente lo rechaza.
- Siempre se imprimen **completas** A, B, las filas de cada servidor y C.
- A y B son puros 1, así que **todos los valores de C deben ser `N`**.

**Variante principal**

```bash
docker exec -it rpc-matriz-cliente ./principal/cliente1 servidor1 servidor2       # N = 10
docker exec -it rpc-matriz-cliente ./principal/cliente1 servidor1 servidor2 3     # N = 3
```
```
Matriz A:
1 1 1
1 1 1
1 1 1

Matriz B:
1 1 1
1 1 1
1 1 1

Resultado parcial servidor 1:
3 3 3

Resultado parcial servidor 2:
3 3 3
3 3 3

Matriz resultado C completa:
3 3 3
3 3 3
3 3 3
```

Con `N` impar el servidor 2 hace una fila más (`bloque = N / 2` se redondea hacia abajo).

**`N` que no cabe en los arreglos**

```bash
docker exec -it rpc-matriz-cliente ./principal/cliente1 servidor1 servidor2 11
```
```
N debe estar entre 1 y 10: los arreglos de matriz.h/matriz2.h son de 100 elementos
```

**Qué calculó cada servidor**

```bash
docker logs rpc-matriz-servidor1     # Servidor 1 calculó filas 0 a 4
docker logs rpc-matriz-servidor2     # Servidor 2 calculó filas 5 a 9
```

**Variante Pruebas** — hay que reiniciar los servidores con esa variante:

```bash
VARIANTE=pruebas docker compose up -d          # bash
$env:VARIANTE="pruebas"; docker compose up -d  # PowerShell

docker exec -it rpc-matriz-cliente ./pruebas/cliente1 servidor1 servidor2         # N = 100
docker exec -it rpc-matriz-cliente ./pruebas/cliente1 servidor1 servidor2 37      # N = 37
docker exec -it rpc-matriz-cliente ./pruebas/cliente1 servidor1 servidor2 101     # se rechaza
```

Con `N = 100`, C sale con 10000 valores iguales a `100`. Para regresar a la principal:
`docker compose up -d` (sin `VARIANTE`).

## Errores encontrados y correcciones

Se ejecutó todo como estaba (en una sola máquina con `localhost`, como se probó originalmente):

| # | Problema | Evidencia | Corrección |
|---|----------|-----------|------------|
| 1 | `cliente1.c` con `N 32`: 32×32 = 1024 números no caben en `A[100]` de `matriz.h`; se escribe fuera del arreglo y se pisan las variables del ciclo | El cliente se queda **colgado** imprimiendo `1 1 1 ...` para siempre | `N` por defecto 10, y se puede pasar como argumento: el cliente calcula el máximo que cabe en los arreglos y rechaza un `N` más grande |
| 2 | `Pruebas/cliente1.c` con `N 100`: A y B de 100×100 son 80 KB y **no caben en un mensaje UDP**. Esto es lo que no dejaba crecer la matriz | `Error en Servidor 1: RPC: Can't encode arguments` | `"udp"` → `"tcp"` |
| 3 | Los dos `cliente1.c` tenían `"localhost"` fijo: solo funcionaban con los servidores en la misma máquina | — | `./cliente1 host1 host2` (sin argumentos sigue usando `localhost`) |
| 4 | `cliente.c` y `clientem.c` no compilan con los `.h` actuales | `error: unknown type name 'matrices'; did you mean 'matrices1'?` | Se dejan como están (intentos anteriores) |
| 5 | Los `.x` no coinciden con los `.h`: `matriz.x` dice `A[4]` y define `MATRIZ_PROG` dos veces; `matriz2.x` dice `A[1024]` | Con `rpcgen matriz.x`: `unknown type name 'matrices1'`, `redefinition of 'matriz_prog_1'` | Docker usa los `.h`/`.c` generados del repo |
| 6 | `servidor.c`, `cliente1.x` y `Servidor2/cliente1.c` están vacíos | — | Se dejan |
| 7 | En `readme.txt` decía `-ltirp` | Error de enlace | `-ltirpc` |
| 8 | Con 10,000×10,000 este diseño no podía funcionar: 400 MB por matriz en arreglos fijos, cada servidor recibe A y B completas aunque solo calcule la mitad, y los servidores trabajan uno por uno | — | Ver [`distribuido/`](distribuido/) |

## En máquinas reales (sin Docker)

```bash
sudo systemctl start rpcbind
gcc -o servidor1 servidor1.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc
gcc -o servidor2 servidor2.c matriz2_svc.c matriz2_xdr.c -I/usr/include/tirpc -ltirpc
gcc -o cliente1 cliente1.c matriz_clnt.c matriz2_clnt.c matriz_xdr.c matriz2_xdr.c -I/usr/include/tirpc -ltirpc

./servidor1                  # máquina 1
./servidor2                  # máquina 2
./cliente1 <ip1> <ip2>       # máquina cliente
```
