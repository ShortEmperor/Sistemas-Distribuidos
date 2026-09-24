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
| `cliente1.c` | **Cliente que funciona** (2 servidores, `N 10`) |
| `servidor1.c`, `servidor2.c` | **Servidores que funcionan** |
| `matriz.h`, `matriz_*.c` / `matriz2.h`, `matriz2_*.c` | Generados por rpcgen para PROG1 / PROG2 (arreglos de 100) |
| `matriz.x`, `matriz2.x` | Definiciones RPC; **no coinciden** con los `.h` que se usan (ver errores) |
| `cliente.c` | Intento anterior (puertos fijos 5001/5002 con `clntudp_create`); no compila |
| `clientem.c` | Intento anterior (un solo tipo `matrices`); no compila |
| `servidor.c`, `cliente1.x` | Vacíos |
| `Servidor1/`, `Servidor2/` | Copias para correr cada servidor en una máquina distinta (arreglos de 4) |
| `Pruebas/` | Variante con `N 100` y arreglos de 10000; `matriz3.x` y `matriz4.x` son borradores para 4 servidores |
| `cliente`, `cliente1`, `*/servidor*` | Binarios viejos (no se usan) |
| `readme.txt` | Readme original |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Docker**

- La imagen compila **2 variantes** usando los `.h`/`.c` generados que ya estaban en el repo:
  - `principal`: archivos de esta carpeta (`N = 10`, arreglos de 100).
  - `pruebas`: carpeta `Pruebas/` (`N = 100`, arreglos de 10000).
- La variable `VARIANTE` elige qué servidores arrancan (por defecto `principal`). Cliente y
  servidores **deben ser de la misma variante** (los tamaños de los arreglos tienen que coincidir).
- Cada servidor arranca `rpcbind` y después su programa. `stdbuf -oL` hace que sus `printf`
  aparezcan en `docker logs`.

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

**Variante principal (N = 10)**

```bash
docker exec -it rpc-matriz-cliente ./principal/cliente1 servidor1 servidor2
```
```
Resultado parcial servidor 1:
10 10 10 10 10 10 10 10 10 10
...
Matriz resultado C completa:
10 10 10 10 10 10 10 10 10 10
...
```

**Qué calculó cada servidor**

```bash
docker logs rpc-matriz-servidor1     # Servidor 1 calculó filas 0 a 4
docker logs rpc-matriz-servidor2     # Servidor 2 calculó filas 5 a 9
```

**Variante Pruebas (N = 100)** — hay que reiniciar los servidores con esa variante:

```bash
VARIANTE=pruebas docker compose up -d          # bash
$env:VARIANTE="pruebas"; docker compose up -d  # PowerShell

docker exec -it rpc-matriz-cliente ./pruebas/cliente1 servidor1 servidor2
```

C sale con 10000 valores iguales a `100`. Para regresar a la principal:
`docker compose up -d` (sin `VARIANTE`).

## Errores encontrados y correcciones

Se ejecutó todo como estaba (en una sola máquina con `localhost`, como se probó originalmente):

| # | Problema | Evidencia | Corrección |
|---|----------|-----------|------------|
| 1 | `cliente1.c` con `N 32`: 32×32 = 1024 números no caben en `A[100]` de `matriz.h`; se escribe fuera del arreglo y se pisan las variables del ciclo | El cliente se queda **colgado** imprimiendo `1 1 1 ...` para siempre | `N 10` (lo máximo que cabe en `A[100]`) |
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
