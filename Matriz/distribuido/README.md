# Multiplicación de matrices distribuida con RPC

Lo que pedía la práctica: multiplicar matrices de **10,000 × 10,000 entre 4 computadoras**
usando RPC. Un cliente reparte las filas entre 4 servidores, **los 4 calculan al mismo
tiempo** y el cliente junta el resultado.

Resultado en Docker (4 servidores en la misma laptop): **10,000 × 10,000 en ~140-150 s**,
con A y B de puros 1 → todos los valores de C son 10000.

## Cómo funciona

1. El cliente llena `A` y `B` con **puros 1**. Así el resultado es predecible: cada
   `C[i][j]` es la suma de `N` productos `1 × 1`, o sea **todos los valores de C son `N`**.
2. Reparte las filas de `A` entre los `K` servidores. Si `N` no es divisible entre `K`, los
   primeros servidores reciben una fila extra.
3. Crea **un hilo por servidor**; cada hilo:
   - abre una conexión **TCP** con su servidor,
   - le manda `N`, su rango de filas, **solo sus filas de A** y **B completa**,
   - espera la respuesta con sus filas de `C` y las copia en su lugar de la matriz final.
4. Cada servidor multiplica sus filas (`C_local = A_local × B`), mide cuánto tardó y regresa
   el resultado con su nombre de host.
5. El cliente espera a los 4 hilos e imprime qué filas hizo cada servidor y cuánto tardó.
6. Con `--verificar` además recalcula todo en una sola máquina y compara.
7. Imprime las matrices **A, B y C completas** (para cualquier `N`) y al final el resumen:
   tiempo total y, con `--verificar`, `CORRECTO`/`ERROR` y el speedup.

```
                     filas de A + B completa         filas de C
                                            ┌────────────┐
               ┌── hilo 1: filas 0..2499 ──►│ servidor1  │──┐
               │                            ├────────────┤  │
 ┌─────────┐   ├── hilo 2: filas 2500..4999►│ servidor2  │──┤
 │ cliente │───┤                            ├────────────┤  ├──► C completa
 │ A, B    │   ├── hilo 3: filas 5000..7499►│ servidor3  │──┤    (en el cliente)
 └─────────┘   │                            ├────────────┤  │
               └── hilo 4: filas 7500..9999►│ servidor4  │──┘
                                            └────────────┘
                    (las 4 llamadas al mismo tiempo)
```

## Diseño

**Interfaz (`matriz.x`)**

```c
struct peticion {
    int n;             /* tamaño N x N */
    int fila_inicio;   /* primera fila que calcula el servidor */
    int fila_fin;      /* última fila + 1 */
    int A<>;           /* solo las filas [fila_inicio, fila_fin) de A */
    int B<>;           /* B completa */
};

struct respuesta {
    int fila_inicio;
    int fila_fin;
    int C<>;              /* filas [fila_inicio, fila_fin) de C */
    double segundos;      /* tiempo de cálculo en el servidor */
    string servidor<64>;  /* hostname del servidor */
};

program MATRIZ_PROG { version MATRIZ_VERS {
    respuesta MULTIPLICAR(peticion) = 1;
} = 1; } = 0x20000101;
```

Todos los servidores tienen el **mismo programa**, igual que en `Practica2_matrices`.

**Decisiones (y qué problema de la versión anterior resuelve cada una)**

| Decisión | Por qué |
|----------|---------|
| **TCP** en lugar de UDP | UDP no deja mandar mensajes grandes (`RPC: Can't encode arguments` con 100×100) |
| **Arreglos de tamaño variable** (`int A<>`) | Con arreglos fijos (`A[100]`, `A[10000]`) se manda siempre todo y un `N` más grande se desborda. Así sirve para cualquier `N` y solo viaja lo necesario |
| A cada servidor **solo sus filas de A**, y regresa **solo sus filas de C** | Antes cada servidor recibía A completa y regresaba C del tamaño completo |
| **Un hilo por servidor** (llamadas en paralelo) | Antes se llamaba a los servidores uno por uno: solo trabajaba una máquina a la vez |
| **`rpcgen -M`** | Los stubs normales usan una variable `static` para el resultado y no sirven con hilos. Con `-M` el resultado va en un argumento y se libera con `xdr_free` |
| Timeout de 1 hora (`clnt_control`) | El timeout por defecto es de 25 s y el cálculo de 10000×10000 tarda minutos |
| Las matrices se imprimen **después** de medir el tiempo, y el resumen va **al final** | Imprimir no es parte del cálculo distribuido; con matrices grandes el resumen es lo único que se ve sin desplazarse |
| Ancho de columna según el número más grande y una escritura (`fwrite`) por fila | Columnas alineadas aunque C tenga números de más cifras que A y B, y rápido aunque sean millones de números |
| A y B de **puros 1** | Resultado predecible (C = puros `N`), fácil de revisar a simple vista o con un script; el cliente lo comprueba en cada ejecución |
| Multiplicación en orden `i-k-j` | Recorre `B` y `C` por filas (mejor uso de la caché) |
| El servidor valida los tamaños recibidos | Si `A` o `B` no miden lo esperado, rechaza la petición en lugar de leer fuera del arreglo |

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `matriz.x` | Interfaz RPC |
| `servidor.c` | Implementación de `MULTIPLICAR` |
| `cliente.c` | Cliente: reparte, llama en paralelo, junta y verifica |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Memoria con N = 10,000** (`int` = 4 bytes → 400 MB por matriz)

| Dónde | Qué guarda | Total |
|-------|-----------|-------|
| cliente | A + B + C | 1.2 GB |
| cada servidor | B + sus filas de A y de C | ~600 MB |

En Docker se midieron ~770 MB en el cliente y ~510 MB en cada servidor durante el cálculo.

## Código explicado

> Para lo básico de RPC (qué hacen `clnt_create`, los stubs, XDR, rpcbind, `svc_register`)
> ver la sección "Código explicado" de [`../../RPC/README.md`](../../RPC/README.md).

### `rpcgen -M`: cómo cambian las funciones

Con `-M` (*multithread*) rpcgen genera funciones que **no usan variables `static`**:

| | Sin `-M` (versiones anteriores) | Con `-M` (esta versión) |
|---|---|---|
| Cliente | `resultado *multiplicar_1(peticion *, CLIENT *)` | `enum clnt_stat multiplicar_1(peticion *, respuesta *res, CLIENT *)` |
| Servidor | `resultado *multiplicar_1_svc(peticion *, struct svc_req *)` | `bool_t multiplicar_1_svc(peticion *, respuesta *res, struct svc_req *)` |
| Liberar resultado | — | el servidor debe definir `matriz_prog_1_freeresult` |

El resultado se escribe en `*res`, que pone quien llama. Así cada hilo del cliente tiene su
propia respuesta.

Los arreglos `int A<>` se convierten en C en una estructura con largo y puntero:
`struct { u_int A_len; int *A_val; } A;`. En XDR viajan como *largo + elementos*
(`xdr_array`), así solo se manda lo que se usa.

### `servidor.c`

**`ahora()`**: hora actual en segundos (con microsegundos) usando `gettimeofday`; sirve para
medir el cálculo.

**`multiplicar_1_svc(peticion *p, respuesta *res, struct svc_req *req)`**:

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `filas = p->fila_fin - p->fila_inicio` | Cuántas filas le tocan |
| 2 | `gethostname`, `printf("[%s] recibi filas ...")` | Registra la petición en su log |
| 3 | `if (filas <= 0 \|\| p->A.A_len != filas*n \|\| p->B.B_len != n*n)` | Valida que A traiga exactamente sus filas y B sea `N×N`; si no, regresa `FALSE` (el cliente recibe un error) en lugar de leer fuera del arreglo |
| 4 | `res->C.C_val = calloc(filas * n, sizeof(int))` | Reserva solo sus filas de C, en ceros |
| 5 | `res->servidor = strdup(host)` | Guarda su nombre en la respuesta |
| 6 | triple ciclo `i`, `k`, `j` | `C[i][j] += A[i][k] * B[k][j]`. El orden **i-k-j** recorre B y C fila por fila (memoria contigua), mucho más rápido que el orden i-j-k para matrices grandes. `A` local empieza en la fila 0: la fila `i` local es la fila `fila_inicio + i` real |
| 7 | `res->segundos = ahora() - t0` | Tiempo de cálculo |
| 8 | `return TRUE` | El código generado codifica `*res` y lo manda |

**`matriz_prog_1_freeresult(...)`**: lo llama el código generado **después** de mandar la
respuesta; `xdr_free` libera `C_val` y `servidor`. Sin esto cada petición dejaría 100 MB sin
liberar (con N = 10000).

### `cliente.c`

**`trabajo`** (estructura): todo lo que necesita un hilo: host, `N`, rango de filas, punteros a
las matrices completas `A`, `B`, `C`, y lo que regresa (si salió bien, tiempos, nombre del
servidor).

**`multiplicar_local(A, B, C, filas, n)`**: la misma multiplicación i-k-j, en el cliente; se
usa para `--verificar`.

**`imprimir(nombre, M, n)`**: imprime la matriz completa. Busca el número más grande para
calcular el ancho de columna, arma cada fila en un buffer con `sprintf(p, "%*d ", ancho, valor)`
y la escribe de una vez con `fwrite`.

**`llamar_servidor(void *arg)`** — lo que hace **cada hilo**:

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `clnt_create(t->host, MATRIZ_PROG, MATRIZ_VERS, "tcp")` | Conexión **TCP** con su servidor |
| 2 | `clnt_control(clnt, CLSET_TIMEOUT, &espera)` | Timeout de 1 hora (el de fábrica es 25 s y el cálculo tarda minutos) |
| 3 | `p.A.A_len = filas * n; p.A.A_val = &t->A[fila_inicio * n]` | **No copia nada**: apunta a sus filas dentro de la A completa del cliente |
| 4 | `p.B.B_len = n * n; p.B.B_val = t->B` | B completa |
| 5 | `multiplicar_1(&p, &res, clnt) != RPC_SUCCESS` | Llamada RPC; se bloquea este hilo mientras el servidor calcula (los otros hilos siguen) |
| 6 | `if (res.C.C_len == filas * n)` + `memcpy(&t->C[fila_inicio * n], res.C.C_val, ...)` | Revisa el tamaño y copia sus filas en su lugar de la C completa. Cada hilo escribe en filas distintas, así que no se pisan |
| 7 | `xdr_free(xdr_respuesta, &res)` | Libera la memoria que reservó XDR al recibir |
| 8 | `t->ok = 1`, tiempos | Resultado para el reporte final |

**`main`**:

| Paso | Qué hace |
|------|----------|
| 1 | Lee argumentos: el primer número es `N`, `--verificar` activa la verificación, `--no-imprimir` evita imprimir las matrices, lo demás son hosts |
| 2 | `malloc` de A, B y C (`N × N` enteros cada una); si falta memoria se termina |
| 3 | Llena A y B con puros 1 (`A[i] = 1; B[i] = 1;`) |
| 4 | Reparte filas: `n / k` a cada servidor y los primeros `n % k` reciben una extra |
| 5 | `pthread_create` para cada servidor con filas → **las K llamadas empiezan al mismo tiempo** |
| 6 | `pthread_join` de todos: espera a que terminen |
| 7 | Imprime por servidor: filas, tiempo de cálculo y tiempo total con red; si alguno falló termina con error |
| 8 | Con `--verificar`: `multiplicar_local` de toda la matriz y `memcmp` contra `C`. Siempre: cuenta cuántos valores de `C` no son `N` (`distintos`) |
| 9 | `imprimir("A")`, `imprimir("B")`, `imprimir("C")`: las 3 matrices completas (salvo con `--no-imprimir`) |
| 10 | Resumen: tiempo total, `Todos los valores de C son N: CORRECTO` (o cuántos no lo son) y, con `--verificar`, `CORRECTO`/`ERROR` contra la versión local y speedup = tiempo local / tiempo distribuido |

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `rpcgen -M matriz.x` | Genera `matriz.h`, `matriz_xdr.c`, `matriz_clnt.c` y `matriz_svc.c` (con `main`) en versión para hilos |
| `gcc -O2 -o servidor servidor.c matriz_svc.c matriz_xdr.c ...` | Servidor; `-O2` optimiza el ciclo de la multiplicación |
| `gcc -O2 -o cliente cliente.c matriz_clnt.c matriz_xdr.c ... -lpthread` | Cliente con hilos |
| `CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./servidor"]` | rpcbind + servidor, con salida línea por línea para `docker logs` |

### `docker-compose.yml`

4 servicios `servidor1` … `servidor4` iguales (misma imagen, `CMD` del Dockerfile) y un
`cliente` con `sleep infinity` para usarlo con `docker exec`.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

| Contenedor | Rol |
|------------|-----|
| `rpc-matdist-servidor1` … `rpc-matdist-servidor4` | servidores |
| `rpc-matdist-cliente` | cliente |

## Pruebas

Uso: `./cliente N [--verificar] [--no-imprimir] servidor1 servidor2 ... servidorK`

- `N`: tamaño de la matriz.
- Siempre se imprimen **A, B y C completas**, sea cual sea `N`.
- `--verificar`: recalcula todo en el cliente, compara e imprime el speedup
  (no usar con `N` muy grande: tarda lo mismo que una sola máquina).
- `--no-imprimir`: no imprime las matrices (para medir tiempos sin generar el texto).
- Se puede usar cualquier cantidad de servidores.

El orden de la salida es siempre: qué hizo cada servidor → matriz A → matriz B → matriz C →
resumen de tiempos.

**Tamaño de la salida**: con `N = 300` son 705 KB; con `N = 1500`, unos 20 MB; con
`N = 10,000`, más de 1 GB. Para `N` grande conviene mandarlo a un archivo y copiarlo:

```bash
docker exec rpc-matdist-cliente sh -c "./cliente 1500 --verificar servidor1 servidor2 servidor3 servidor4 > salida.txt"
docker cp rpc-matdist-cliente:/matriz/salida.txt .
```

> En PowerShell 5.1 evitar `docker exec ... > salida.txt` directo: `>` guarda el archivo en
> UTF-16 y lo hace el doble de grande. Con `sh -c "... > salida.txt"` la redirección ocurre
> dentro del contenedor.

**Matriz pequeña**

```bash
docker exec -it rpc-matdist-cliente ./cliente 4 --verificar servidor1 servidor2 servidor3 servidor4
```
```
Multiplicando matrices de 4x4 con 4 servidores
servidor1 (servidor1): filas 0 a 0, calculo 0.000 s, total con red 0.008 s
servidor2 (servidor2): filas 1 a 1, calculo 0.000 s, total con red 0.008 s
servidor3 (servidor3): filas 2 a 2, calculo 0.000 s, total con red 0.008 s
servidor4 (servidor4): filas 3 a 3, calculo 0.000 s, total con red 0.006 s
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
Tiempo total (envio + calculo + recoleccion): 0.007 s
Todos los valores de C son 4 (= N): CORRECTO
Verificacion contra version local: CORRECTO
Tiempo local (1 maquina): 0.000 s  ->  speedup: 0.00x
```

**Speedup (N = 1500, sin imprimir las matrices)**

```bash
docker exec -it rpc-matdist-cliente ./cliente 1500 --verificar --no-imprimir servidor1 servidor2 servidor3 servidor4
```
```
servidor1 (servidor1): filas 0 a 374, calculo 0.684 s, total con red 0.738 s
servidor2 (servidor2): filas 375 a 749, calculo 0.701 s, total con red 0.761 s
servidor3 (servidor3): filas 750 a 1124, calculo 0.688 s, total con red 0.748 s
servidor4 (servidor4): filas 1125 a 1499, calculo 0.664 s, total con red 0.726 s
Tiempo total (envio + calculo + recoleccion): 0.762 s
Todos los valores de C son 1500 (= N): CORRECTO
Verificacion contra version local: CORRECTO
Tiempo local (1 maquina): 2.207 s  ->  speedup: 2.89x
```

**Tamaño de la práctica (N = 10,000)** — unos 3 minutos; la salida con las matrices pasa de
1 GB, así que va a un archivo (o usar `--no-imprimir` para ver solo los tiempos)

```bash
docker exec rpc-matdist-cliente sh -c "./cliente 10000 servidor1 servidor2 servidor3 servidor4 > salida.txt"
docker exec rpc-matdist-cliente sh -c "head -5 salida.txt; tail -2 salida.txt"
```
```
Multiplicando matrices de 10000x10000 con 4 servidores
servidor1 (servidor1): filas 0 a 2499, calculo 145.734 s, total con red 147.671 s
servidor2 (servidor2): filas 2500 a 4999, calculo 144.895 s, total con red 146.937 s
servidor3 (servidor3): filas 5000 a 7499, calculo 146.012 s, total con red 147.863 s
servidor4 (servidor4): filas 7500 a 9999, calculo 145.350 s, total con red 147.196 s
Tiempo total (envio + calculo + recoleccion): 147.864 s
Todos los valores de C son 10000 (= N): CORRECTO
```

Mientras corre se puede ver que los 4 trabajan al mismo tiempo:

```bash
docker stats
```

**Otros casos**

```bash
# N no divisible entre los servidores (7 filas entre 3)
docker exec -it rpc-matdist-cliente ./cliente 7 --verificar servidor1 servidor2 servidor3

# más servidores que filas
docker exec -it rpc-matdist-cliente ./cliente 2 --verificar servidor1 servidor2 servidor3 servidor4

# un servidor que no existe: se reporta y el cliente termina con error
docker exec -it rpc-matdist-cliente ./cliente 10 servidor1 noexiste

# qué calculó cada servidor
docker logs rpc-matdist-servidor1
```

## En máquinas reales (sin Docker)

En las 5 máquinas:

```bash
sudo systemctl start rpcbind
rpcgen -M matriz.x
gcc -O2 -o servidor servidor.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc
gcc -O2 -o cliente cliente.c matriz_clnt.c matriz_xdr.c -I/usr/include/tirpc -ltirpc -lpthread
```

```bash
./servidor                                # en las 4 máquinas servidoras
./cliente 10000 ip1 ip2 ip3 ip4           # en la máquina cliente
```
