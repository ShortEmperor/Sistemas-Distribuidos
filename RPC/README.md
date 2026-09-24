# Calculadora RPC

Práctica de RPC (Sun RPC / ONC RPC) en C: un **servidor** ofrece las 4 operaciones básicas
(suma, resta, multiplicación y división) y un **cliente** remoto las invoca como si fueran
funciones locales.

> En esta versión las 4 operaciones están en el **mismo servidor**. La versión donde cada
> operación está en una computadora distinta está en [`distribuido/`](distribuido/).

## Cómo funciona

1. `calculadora.x` describe la interfaz: una estructura `numeros { float a; float b; }` y un
   programa RPC con 4 procedimientos.
2. `rpcgen calculadora.x` genera el código de red:
   - `calculadora.h`: tipos y constantes compartidas.
   - `calculadora_xdr.c`: serialización XDR de `numeros` (formato independiente de la máquina).
   - `calculadora_clnt.c`: *stubs* del cliente (`suma_1()`, `resta_1()`, ...).
   - `calculadora_svc.c`: `main` del servidor, que registra el programa y despacha cada llamada.
3. El servidor se registra en **rpcbind** (puerto 111), que anota en qué puerto escucha.
4. El cliente llama a `clnt_create(host, ...)`: le pregunta a rpcbind de `host` el puerto del
   programa y abre la conexión (UDP).
5. Cada `suma_1(&nums, clnt)` serializa los argumentos, los manda, espera la respuesta y la
   deserializa.

```
  cliente                                   servidor
 ┌───────────────┐   1. ¿puerto de 0x20000001?  ┌──────────────┐
 │ cliente.c     │ ───────────────────────────► │ rpcbind :111 │
 │               │ ◄─────────────────────────── │              │
 │ suma_1()      │   2. SUMA(a, b)  (XDR/UDP)   │ servidor     │
 │ resta_1()     │ ───────────────────────────► │  suma_1_svc  │
 │ ...           │ ◄─────────────────────────── │  resta_1_svc │
 └───────────────┘        resultado float       └──────────────┘
```

## Diseño

**Interfaz (`calculadora.x`)**

| Procedimiento    | Número | Argumento | Resultado |
|------------------|:------:|-----------|-----------|
| `SUMA`           | 1      | `numeros` | `float`   |
| `RESTA`          | 2      | `numeros` | `float`   |
| `MULTIPLICACION` | 3      | `numeros` | `float`   |
| `DIVISION`       | 4      | `numeros` | `float`   |

Programa `OPERACIONES_PROG = 0x20000001`, versión `1`.

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `calculadora.x` | Definición de la interfaz RPC |
| `calculadora_srv.c` | **Implementación real** de las 4 operaciones (servidor) |
| `cliente.c` | **Cliente**: pide 2 números y llama las 4 operaciones |
| `calculadora.h`, `calculadora_clnt.c`, `calculadora_svc.c`, `calculadora_xdr.c` | Generados por `rpcgen` |
| `calculadora_server.c`, `calculadora_client.c`, `Makefile.calculadora` | Plantillas vacías de `rpcgen` (no se usan) |
| `cliente`, `servidor` | Binarios viejos (no se usan; Docker compila de nuevo) |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Decisiones**

- La división entre 0 regresa `0` en el servidor (un `float` no puede indicar error); es el
  cliente el que revisa `b == 0` y muestra el mensaje.
- El cliente revisa que cada llamada no regrese `NULL` (servidor caído o sin respuesta).

**Docker**

- Imagen `debian:bookworm-slim` con `gcc`, `libtirpc-dev` (RPC), `rpcsvc-proto` (`rpcgen`),
  `rpcbind` y `netbase`.
- Se compila con los archivos correctos (`calculadora_srv.c` + `cliente.c`), no con el Makefile.
- Contenedor `servidor`: arranca `rpcbind` y después `./servidor`.
- Contenedor `cliente`: se queda encendido (`sleep infinity`) para entrar con `docker exec`.
- Los dos contenedores están en la misma red de Docker y se encuentran por nombre (`servidor`).

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build     # construir y levantar
docker compose ps                # ver contenedores
docker compose down              # apagar
```

| Contenedor | Rol |
|------------|-----|
| `rpc-calc-servidor` | rpcbind + servidor con las 4 operaciones |
| `rpc-calc-cliente`  | cliente |

## Pruebas

**Uso interactivo**

```bash
docker exec -it rpc-calc-cliente ./cliente servidor
```

**Sin escribir los números a mano**

```bash
printf "10\n4\n" | docker exec -i rpc-calc-cliente ./cliente servidor
```
```
Ingresa el primer numero: Ingresa el segundo numero: Suma: 14.000000
Resta: 6.000000
Multiplicacion: 40.000000
Division: 2.500000
```

**División entre 0**

```bash
printf "7.5\n0\n" | docker exec -i rpc-calc-cliente ./cliente servidor
```
```
... Division: no se puede dividir entre 0
```

**Ver el programa registrado en rpcbind** (`536870913` = `0x20000001`)

```bash
docker exec rpc-calc-servidor rpcinfo -p
```

**Servidor que se cae a media ejecución** (el cliente conecta, se congela el servidor y
después se mandan los números):

```bash
docker exec rpc-calc-cliente sh -c '(sleep 4; printf "1\n1\n") | ./cliente servidor' &
sleep 2; docker pause rpc-calc-servidor; wait; docker unpause rpc-calc-servidor
```
```
Error en Suma: RPC: Timed out
```

**Host que no existe**

```bash
printf "1\n1\n" | docker exec -i rpc-calc-cliente ./cliente noexiste
```
```
noexiste: RPC: Unknown host
```

## Errores encontrados y correcciones

| Problema | Evidencia | Corrección |
|----------|-----------|------------|
| `Makefile.calculadora` compila las plantillas vacías de rpcgen: el servidor resultante siempre regresa 0 | `calculadora_server.c` solo tiene `/* insert server code here */` | Se compila a mano con `calculadora_srv.c` |
| `cliente.c` no revisaba si la llamada regresaba `NULL` | Con el servidor congelado: `Segmentation fault` (exit 139) | Se revisa `NULL` y se muestra `clnt_perror` |
| División entre 0 se mostraba como resultado válido | `Division: 0.000000` | El cliente muestra `no se puede dividir entre 0` |
| En la imagen faltaba `netbase` (`/etc/services`, `/etc/protocols`) | rpcbind no abría el puerto 111 y todo daba `RPC: Unknown host` | Se agregó `netbase` al Dockerfile |
| Las 4 operaciones están en el mismo servidor (la práctica pedía una por computadora) | — | Ver [`distribuido/`](distribuido/) |

## En máquinas reales (sin Docker)

```bash
sudo systemctl start rpcbind
gcc -o servidor calculadora_svc.c calculadora_srv.c calculadora_xdr.c -I/usr/include/tirpc -ltirpc
gcc -o cliente cliente.c calculadora_clnt.c calculadora_xdr.c -I/usr/include/tirpc -ltirpc

./servidor                  # en la máquina servidor
./cliente <ip-servidor>     # en la máquina cliente
```
