# Practica2_matrices: multiplicación de matrices con RPC (4 servidores)

Plantilla original de la práctica de matrices con RPC: el cliente reparte las filas de
`C = A × B` entre **4 servidores** que tienen el mismo programa RPC.

> `readme.txt` es el readme original y se deja igual.
> Para la versión que llega a 10,000 × 10,000 ver [`../Matriz/distribuido/`](../Matriz/distribuido/).

## Cómo funciona

1. Los 4 servidores corren el **mismo programa** (`MATRIZ_PROG = 0x20000001`), cada uno en su
   máquina.
2. El cliente (`clientem.c`) llena las matrices:
   - `A[i][j] = i + j + 1`
   - `B[i][j] = (i + 1) * (j + 1)`
3. Divide las filas en 4 bloques de `N / 4` (el último servidor también hace las que sobran).
4. Para cada servidor, **uno después de otro**: se conecta, le manda `n`, `fila_inicio`,
   `fila_fin`, A y B completas, y muestra las filas que le regresa.
5. El servidor (`servidor.c`) calcula solo las filas `[fila_inicio, fila_fin)` de `C`.

```
 ┌──────────┐  A, B, filas 0..0   ┌───────────┐
 │ cliente  │ ──────────────────► │ servidor1 │
 │          │  A, B, filas 1..1   ├───────────┤
 │ (llama   │ ──────────────────► │ servidor2 │
 │ uno por  │  A, B, filas 2..2   ├───────────┤
 │ uno)     │ ──────────────────► │ servidor3 │
 │          │  A, B, filas 3..3   ├───────────┤
 │          │ ──────────────────► │ servidor4 │
 └──────────┘                     └───────────┘
        (N = 4: una fila por servidor)
```

## Diseño

**Interfaz (`matriz.x`)**

```c
struct matrices  { int n; int fila_inicio; int fila_fin; int A[10000]; int B[10000]; };
struct resultado { int n; int fila_inicio; int fila_fin; int C[10000]; };

program MATRIZ_PROG { version MATRIZ_VERS {
    resultado MULTIPLICAR(matrices) = 1;
} = 1; } = 0x20000001;
```

- Arreglos **fijos de 10000**: el `N` máximo es 100, y en cada llamada viajan siempre los
  10000 números de A y de B (80 KB), aunque `N` sea 4.
- `N = 4` y `SERV = 4` están fijos en `clientem.c`.

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `matriz.x` | Interfaz RPC |
| `servidor.c` | Implementación de `MULTIPLICAR` |
| `clientem.c` | Cliente: 4 servidores |
| `readme.txt` | Readme original |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

En esta carpeta no estaban los archivos que genera `rpcgen`; la imagen de Docker los genera
al construirse.

**Docker**

- Una imagen para los 5 contenedores: `rpcgen matriz.x` + compilación.
- Cada servidor arranca `rpcbind` y después `./servidor` (con `stdbuf -oL` para que sus
  `printf` salgan en `docker logs`).
- El cliente se queda encendido para usarlo con `docker exec`.

## Código explicado

> Para lo básico de RPC (qué hacen `clnt_create`, los stubs, XDR, rpcbind, `svc_register`)
> ver la sección "Código explicado" de [`../RPC/README.md`](../RPC/README.md).

Las matrices se guardan en **arreglos de una dimensión**: el elemento `(i, j)` de una matriz
`N × N` está en la posición `i*N + j`.

### `matriz.x`

- `struct matrices`: lo que se manda al servidor — `n`, el rango de filas `[fila_inicio,
  fila_fin)` y las matrices `A[10000]` y `B[10000]`.
- `struct resultado`: lo que regresa — `n`, el rango de filas y `C[10000]`.
- Un programa `MATRIZ_PROG` (`0x20000001`) con el procedimiento `MULTIPLICAR`. Los 4
  servidores corren este mismo programa.

### `clientem.c`

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `#define N 4`, `#define SERV 4` | Matriz de 4 × 4 entre 4 servidores |
| 2 | `servidores[SERV] = {"192.168.229.48", ...}` + `if (argc > SERV)` | IPs del laboratorio por defecto; si se pasan 4 argumentos se usan esos hosts |
| 3 | `m.A[i*N + j] = i + j + 1` y `m.B[i*N + j] = (i+1)*(j+1)` | Llena A y B y las imprime |
| 4 | `bloque = N / SERV` | Filas por servidor (1) |
| 5 | `m.fila_inicio = s * bloque; m.fila_fin = (s + 1) * bloque;` | Rango del servidor `s`; el último (`s == SERV - 1`) llega hasta `N` para no perder filas |
| 6 | `clnt_create(servidores[s], MATRIZ_PROG, MATRIZ_VERS, "tcp")` | Conecta al servidor `s` |
| 7 | `res = multiplicar_1(&m, clnt)` | Manda A y B completas + su rango y espera sus filas de C |
| 8 | ciclo `for (i = m.fila_inicio; i < m.fila_fin; i++)` | Imprime las filas que calculó ese servidor |
| 9 | `clnt_destroy(clnt)` | Cierra y pasa al siguiente servidor |

El cliente no junta una matriz C completa: imprime el resultado parcial de cada servidor, en
orden, y juntos forman C.

### `servidor.c`

```c
resultado *multiplicar_1_svc(matrices *m, struct svc_req *req) {
    static resultado res;                       // static: se lee después de regresar
    int n = m->n;
    res.n = n; res.fila_inicio = m->fila_inicio; res.fila_fin = m->fila_fin;
    for (i = m->fila_inicio; i < m->fila_fin; i++)   // solo sus filas
        for (j = 0; j < n; j++) {
            res.C[i*n + j] = 0;
            for (k = 0; k < n; k++)
                res.C[i*n + j] += m->A[i*n + k] * m->B[k*n + j];   // fila i de A · columna j de B
        }
    printf("Servidor calculó filas %d a %d\n", ...);
    return &res;
}
```

### Archivos generados (solo existen dentro de la imagen)

`rpcgen matriz.x` genera `matriz.h` (tipos y constantes), `matriz_xdr.c` (`xdr_matrices` y
`xdr_resultado`: mandan `n`, el rango y **los 10000 elementos de cada arreglo**, siempre),
`matriz_clnt.c` (stub `multiplicar_1`) y `matriz_svc.c` (`main` que registra el programa y
despachador).

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `apt-get install gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase ...` | Compilador, biblioteca RPC, rpcgen, rpcbind y `/etc/services` |
| `COPY matriz.x servidor.c clientem.c ./` | Solo los 3 archivos fuente |
| `rpcgen matriz.x` | Genera los archivos de RPC |
| `gcc -o servidor ...` / `gcc -o cliente clientem.c ...` | Compila servidor y cliente |
| `CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./servidor"]` | rpcbind + servidor, con salida línea por línea para `docker logs` |

### `docker-compose.yml`

4 servicios `servidor1` … `servidor4` iguales y un `cliente` con `sleep infinity` para usarlo
con `docker exec`. Todos en la misma red, se encuentran por nombre.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

| Contenedor | Rol |
|------------|-----|
| `p2-matriz-servidor1` … `p2-matriz-servidor4` | servidores |
| `p2-matriz-cliente` | cliente |

## Pruebas

```bash
docker exec -it p2-matriz-cliente ./cliente servidor1 servidor2 servidor3 servidor4
```
```
Matriz A:
1 2 3 4
2 3 4 5
3 4 5 6
4 5 6 7

Matriz B:
1 2 3 4
2 4 6 8
3 6 9 12
4 8 12 16

Resultado parcial servidor 1:
30 60 90 120

Resultado parcial servidor 2:
40 80 120 160

Resultado parcial servidor 3:
50 100 150 200

Resultado parcial servidor 4:
60 120 180 240
```

**Qué fila calculó cada servidor**

```bash
docker logs p2-matriz-servidor1     # Servidor calculó filas 0 a 0
docker logs p2-matriz-servidor4     # Servidor calculó filas 3 a 3
```

**Sin argumentos** usa las IPs originales del laboratorio, que no existen dentro de Docker:

```bash
docker exec -it p2-matriz-cliente ./cliente
# 192.168.229.48: RPC: Remote system error - Connection refused
```

## Errores encontrados y correcciones

Se ejecutó `clientem.c` como estaba, corrigiendo un error a la vez para ver el siguiente:

| # | Problema | Evidencia | Corrección |
|---|----------|-----------|------------|
| 1 | IPs `192.168.229.48/.50` fijas en el código | `192.168.229.48: RPC: Timed out` | `./cliente host1 host2 host3 host4` (sin argumentos usa las IPs) |
| 2 | Los arreglos fijos `A[10000]`/`B[10000]` siempre viajan completos (80 KB) y **no caben en UDP**, aunque `N` sea 4 | `Error en RPC: RPC: Can't encode arguments` | `"udp"` → `"tcp"` |
| 3 | `servidores[]` tenía 2 IPs (las otras 2 comentadas) pero el ciclo iba de 0 a 3: `servidores[2]` y `servidores[3]` eran basura | `Segmentation fault` (exit 139) | `servidores[SERV]` con las 4 IPs y `SERV = 4` en el ciclo |
| 4 | Si `N` no era divisible entre 4 se perdían las últimas filas (`bloque = N / 4`) | — | El último servidor hace las filas que sobran |

## En máquinas reales (sin Docker)

En las 5 máquinas:

```bash
sudo systemctl start rpcbind
rpcgen matriz.x
gcc -o servidor servidor.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc
gcc -o cliente clientem.c matriz_clnt.c matriz_xdr.c -I/usr/include/tirpc -ltirpc
```

```bash
./servidor                        # en las 4 máquinas servidoras
./cliente ip1 ip2 ip3 ip4         # en la máquina cliente
```
