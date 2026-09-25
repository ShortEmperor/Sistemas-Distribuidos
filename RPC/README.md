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

## Código explicado

### `calculadora.x` — la interfaz

```c
struct numeros { float a; float b; };        // los 2 operandos viajan juntos

program OPERACIONES_PROG {                   // nombre del programa
    version OPERACIONES_VERS {               // versión (permite tener varias)
        float SUMA(numeros) = 1;             // procedimiento número 1
        float RESTA(numeros) = 2;
        float MULTIPLICACION(numeros) = 3;
        float DIVISION(numeros) = 4;
    } = 1;
} = 0x20000001;                              // número del programa (rango 0x20000000-0x3FFFFFFF es para usuarios)
```

No es código C: es el lenguaje de `rpcgen`. Un procedimiento RPC solo recibe **un** argumento,
por eso `a` y `b` van dentro de la estructura `numeros`. Cliente y servidor se identifican por
la tripleta *(número de programa, versión, número de procedimiento)*.

### `cliente.c` — el cliente

| Paso | Código | Qué hace |
|------|--------|----------|
| 1 | `if (argc < 2)` | Exige el nombre o IP del servidor como argumento |
| 2 | `clnt_create(host, OPERACIONES_PROG, OPERACIONES_VERS, "udp")` | Le pregunta al **rpcbind** de `host` (puerto 111) en qué puerto está el programa `0x20000001` versión 1 y crea un *handle* `CLIENT*` para hablar con él por UDP. Si falla regresa `NULL` y `clnt_pcreateerror` imprime el motivo (`Unknown host`, `Program not registered`, `Timed out`...) |
| 3 | `scanf("%f", &nums.a)` ... | Lee los 2 números en la estructura `numeros` |
| 4 | `resultado = suma_1(&nums, clnt)` | Llama al *stub* generado: serializa `nums`, lo manda, espera y deserializa la respuesta. Regresa un **puntero** a un `float` que vive dentro del stub |
| 5 | `if (resultado == NULL)` | La llamada falló (servidor caído, timeout de 25 s...). `clnt_perror` explica por qué y se termina |
| 6 | lo mismo con `resta_1`, `multiplicacion_1`, `division_1` | Una llamada RPC por operación, las 4 al mismo servidor |
| 7 | `if (nums.b == 0)` | El servidor regresa 0 al dividir entre 0; el cliente muestra el aviso en lugar del 0 |
| 8 | `clnt_destroy(clnt)` | Cierra la conexión y libera el handle |

### `calculadora_srv.c` — las operaciones del servidor

```c
float *suma_1_svc(numeros *argp, struct svc_req *rqstp) {
    static float resultado;            // static: tiene que seguir existiendo al salir
    resultado = argp->a + argp->b;
    return &resultado;
}
```

- El nombre `suma_1_svc` lo decide rpcgen: `<procedimiento en minúsculas>_<versión>_svc`.
- `argp` ya llega **deserializado**: el código de red lo decodificó antes de llamar la función.
- `rqstp` trae datos de la petición (quién llama, qué procedimiento); aquí no se usa.
- El resultado se regresa como **puntero a una variable `static`**: después de que la función
  termina, el despachador de `calculadora_svc.c` todavía tiene que leerlo para mandarlo. Una
  variable local normal ya no existiría.
- `division_1_svc` revisa `b == 0` y en ese caso regresa 0 (un `float` no tiene forma de
  indicar error).

### Archivos generados por `rpcgen calculadora.x`

**`calculadora.h`** — lo comparten cliente y servidor:
- `struct numeros` + `typedef`.
- `#define OPERACIONES_PROG 0x20000001`, `OPERACIONES_VERS 1`, `SUMA 1`, `RESTA 2`, ...
- Prototipos de los stubs del cliente (`suma_1(numeros *, CLIENT *)`) y de las funciones que
  debe implementar el servidor (`suma_1_svc(numeros *, struct svc_req *)`).

**`calculadora_xdr.c`** — serialización:
```c
bool_t xdr_numeros(XDR *xdrs, numeros *objp) {
    if (!xdr_float(xdrs, &objp->a)) return FALSE;
    if (!xdr_float(xdrs, &objp->b)) return FALSE;
    return TRUE;
}
```
La **misma función codifica y decodifica**: `xdrs` indica la dirección (`XDR_ENCODE` al mandar,
`XDR_DECODE` al recibir). XDR es un formato estándar (big-endian, múltiplos de 4 bytes), así
funciona entre máquinas con distinta arquitectura.

**`calculadora_clnt.c`** — stubs del cliente:
```c
static struct timeval TIMEOUT = { 25, 0 };
float *suma_1(numeros *argp, CLIENT *clnt) {
    static float clnt_res;
    memset(&clnt_res, 0, sizeof(clnt_res));
    if (clnt_call(clnt, SUMA, xdr_numeros, argp, xdr_float, &clnt_res, TIMEOUT) != RPC_SUCCESS)
        return NULL;
    return &clnt_res;
}
```
`clnt_call` hace todo el trabajo: codifica el argumento con `xdr_numeros`, manda la petición
con el número de procedimiento `SUMA`, espera hasta 25 s y decodifica la respuesta con
`xdr_float`. El resultado queda en una variable `static`, así que la siguiente llamada lo
sobreescribe (y no sirve con hilos).

**`calculadora_svc.c`** — `main` del servidor y despachador:
- `main`:
  1. `pmap_unset(OPERACIONES_PROG, OPERACIONES_VERS)`: borra de rpcbind un registro viejo del
     mismo programa (por si el servidor anterior no terminó bien).
  2. `svcudp_create(RPC_ANYSOCK)`: abre un socket UDP en un puerto libre cualquiera.
  3. `svc_register(transp, PROG, VERS, operaciones_prog_1, IPPROTO_UDP)`: le dice a rpcbind
     "el programa 0x20000001 v1 está en este puerto" y que las peticiones las atienda
     `operaciones_prog_1`.
  4. Lo mismo con TCP (`svctcp_create`), así el cliente puede usar cualquiera de los dos.
  5. `svc_run()`: ciclo infinito que espera peticiones y llama al despachador.
- `operaciones_prog_1(rqstp, transp)` (despachador):
  1. `switch (rqstp->rq_proc)`: según el número de procedimiento elige la función XDR del
     argumento, la del resultado y la función a llamar (`suma_1_svc`, ...). `NULLPROC` (0)
     responde vacío: sirve como "ping" (`rpcinfo -u`). Un número desconocido responde
     `svcerr_noproc`.
  2. `svc_getargs`: decodifica el argumento.
  3. Llama la función (`suma_1_svc`).
  4. `svc_sendreply`: codifica el resultado y lo manda.
  5. `svc_freeargs`: libera la memoria del argumento.

### Plantillas de `rpcgen -a` (no se usan)

- `calculadora_server.c`: las 4 funciones `*_svc` con `/* insert server code here */`; como
  el resultado es un `static float` sin asignar, siempre regresan 0.
- `calculadora_client.c`: llama las 4 operaciones con argumentos sin inicializar y no imprime
  nada.
- `Makefile.calculadora`: compila justo esas dos plantillas (y usa `-lnsl`, que ya no trae RPC
  en distribuciones actuales).

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `FROM debian:bookworm-slim` | Sistema base pequeño |
| `apt-get install gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase ...` | Compilador; **libtirpc** (la biblioteca RPC, ya no viene en glibc); **rpcgen**; **rpcbind**; **netbase** (`/etc/services` y `/etc/protocols`, sin ellos rpcbind no abre el puerto 111); herramientas de red para depurar |
| `COPY *.c *.h *.x ./` | Copia el código a `/rpc` |
| `gcc -o servidor calculadora_svc.c calculadora_srv.c calculadora_xdr.c -I/usr/include/tirpc -ltirpc` | Servidor = `main`/despachador + operaciones + XDR |
| `gcc -o cliente cliente.c calculadora_clnt.c calculadora_xdr.c ...` | Cliente = programa + stubs + XDR |
| `CMD ["sh", "-c", "rpcbind && exec ./servidor"]` | Al arrancar: primero rpcbind (se va a segundo plano) y después el servidor como proceso principal |

### `docker-compose.yml`

- `name: rpc-calculadora`: nombre del proyecto (prefijo de imágenes y red).
- `x-rpc: &rpc` ... `<<: *rpc`: bloque común (construir con el `Dockerfile` de la carpeta y
  usar la red `rpc`) que se reutiliza en los 2 servicios.
- `servidor`: usa el `CMD` del Dockerfile. `hostname: servidor` y el nombre del servicio hacen
  que el cliente lo encuentre como `servidor` (DNS interno de Docker).
- `cliente`: `command: ["sleep", "infinity"]` para que no haga nada y se quede encendido;
  el cliente se ejecuta a mano con `docker exec`. `depends_on` lo arranca después del servidor.

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
