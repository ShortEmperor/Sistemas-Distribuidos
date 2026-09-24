# Calculadora RPC distribuida

Lo que pedía la práctica: **cada operación en una computadora distinta**.
4 servidores (suma, resta, multiplicación, división) y un cliente que le manda cada operación
a la máquina que le corresponde.

## Cómo funciona

1. `operaciones.x` define **4 programas RPC distintos**, uno por operación, cada uno con un
   solo procedimiento.
2. El mismo ejecutable `servidor` se usa en las 4 máquinas, pero cada una **registra en
   rpcbind solo el programa de su operación** (`./servidor suma`, `./servidor resta`, ...).
3. El cliente abre 4 conexiones, una a cada máquina, pide los 2 números y llama cada
   operación en su máquina.
4. Si se le pide una operación a la máquina equivocada, esa máquina responde
   `RPC: Program not registered`: realmente solo sabe hacer su operación.

```
                           ┌───────────────────────────┐
                    ┌─────►│ suma            0x20000011│
                    │      └───────────────────────────┘
 ┌──────────┐       │      ┌───────────────────────────┐
 │ cliente  │───────┼─────►│ resta           0x20000012│
 │ a, b     │       │      └───────────────────────────┘
 └──────────┘       │      ┌───────────────────────────┐
                    ├─────►│ multiplicacion  0x20000013│
                    │      └───────────────────────────┘
                    │      ┌───────────────────────────┐
                    └─────►│ division        0x20000014│
                           └───────────────────────────┘
```

## Diseño

**Interfaz (`operaciones.x`)**

| Programa | Número | Procedimiento |
|----------|--------|---------------|
| `SUMA_PROG` | `0x20000011` | `float SUMA(numeros)` |
| `RESTA_PROG` | `0x20000012` | `float RESTA(numeros)` |
| `MULTIPLICACION_PROG` | `0x20000013` | `float MULTIPLICACION(numeros)` |
| `DIVISION_PROG` | `0x20000014` | `float DIVISION(numeros)` |

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `operaciones.x` | Interfaz RPC: 4 programas |
| `servidor.c` | Implementación de las 4 operaciones + `main` que registra **solo** la operación indicada |
| `cliente.c` | Cliente: `./cliente host_suma host_resta host_multiplicacion host_division` |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**Decisiones**

- **Un `.x` con 4 programas** en lugar de 4 archivos `.x`: la estructura `numeros` se define
  una sola vez (con 4 `.x` el cliente tendría 4 `xdr_numeros` repetidos al enlazar).
- **`rpcgen -m`**: genera los despachadores (`suma_prog_1`, `resta_prog_1`, ...) **sin
  `main`**. Así `servidor.c` tiene su propio `main` que elige qué programa registrar según el
  argumento. El `main` que genera rpcgen normalmente registraría los 4 programas en la misma
  máquina.
- Cada servidor imprime en su log qué operación atendió y el resultado, para comprobar qué
  máquina hizo cada cosa.
- Igual que en la versión original: la división entre 0 regresa `0` y el cliente muestra el
  mensaje de error; el cliente revisa `NULL` en cada llamada.

**Docker**

- Una sola imagen para los 5 contenedores. Los archivos de rpcgen se generan al construir la
  imagen.
- Cada servidor recibe su operación con la variable de entorno `OPERACION` del compose.
- Los contenedores se llaman igual que su operación, así el cliente los encuentra por nombre.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

| Contenedor | Rol |
|------------|-----|
| `rpc-dist-suma` | servidor de suma |
| `rpc-dist-resta` | servidor de resta |
| `rpc-dist-multiplicacion` | servidor de multiplicación |
| `rpc-dist-division` | servidor de división |
| `rpc-dist-cliente` | cliente |

## Pruebas

**Uso interactivo**

```bash
docker exec -it rpc-dist-cliente ./cliente suma resta multiplicacion division
```

**Sin escribir los números a mano**

```bash
printf "10\n4\n" | docker exec -i rpc-dist-cliente ./cliente suma resta multiplicacion division
```
```
Ingresa el primer numero: Ingresa el segundo numero: Suma (suma): 14.000000
Resta (resta): 6.000000
Multiplicacion (multiplicacion): 40.000000
Division (division): 2.500000
```

**Qué atendió cada máquina**

```bash
docker logs rpc-dist-suma
docker logs rpc-dist-division
```
```
[suma] Servidor de suma listo (programa 0x20000011)
[suma] suma(10.000000, 4.000000) = 14.000000
```

**Cada máquina tiene registrado solo su programa**

```bash
docker exec rpc-dist-suma rpcinfo -p       # solo 536870929 (0x20000011)
docker exec rpc-dist-resta rpcinfo -p      # solo 536870930 (0x20000012)
```

**Pedirle la suma a la máquina de resta**

```bash
printf "1\n1\n" | docker exec -i rpc-dist-cliente ./cliente resta resta multiplicacion division
```
```
resta: RPC: Program not registered
```

**División entre 0**

```bash
printf "7.5\n0\n" | docker exec -i rpc-dist-cliente ./cliente suma resta multiplicacion division
```
```
... Division (division): no se puede dividir entre 0
```

## En máquinas reales (sin Docker)

En las 5 máquinas:

```bash
sudo systemctl start rpcbind
rpcgen -h operaciones.x -o operaciones.h
rpcgen -c operaciones.x -o operaciones_xdr.c
rpcgen -l operaciones.x -o operaciones_clnt.c
rpcgen -m operaciones.x -o operaciones_svc.c
gcc -o servidor servidor.c operaciones_svc.c operaciones_xdr.c -I/usr/include/tirpc -ltirpc
gcc -o cliente cliente.c operaciones_clnt.c operaciones_xdr.c -I/usr/include/tirpc -ltirpc
```

```bash
./servidor suma             # máquina 1
./servidor resta            # máquina 2
./servidor multiplicacion   # máquina 3
./servidor division         # máquina 4
./cliente ip1 ip2 ip3 ip4   # máquina cliente
```
