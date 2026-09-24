# Multiplicación de matrices distribuida con RPC

Lo que pedía la práctica: multiplicar matrices de **10,000 × 10,000 entre 4 computadoras**
usando RPC. Un cliente reparte las filas entre 4 servidores, **los 4 calculan al mismo
tiempo** y el cliente junta el resultado.

Resultado en Docker (4 servidores en la misma laptop): **10,000 × 10,000 en 140 s**.

## Cómo funciona

1. El cliente genera `A` y `B` (enteros del 0 al 9, semilla fija para que siempre salgan
   iguales).
2. Reparte las filas de `A` entre los `K` servidores. Si `N` no es divisible entre `K`, los
   primeros servidores reciben una fila extra.
3. Crea **un hilo por servidor**; cada hilo:
   - abre una conexión **TCP** con su servidor,
   - le manda `N`, su rango de filas, **solo sus filas de A** y **B completa**,
   - espera la respuesta con sus filas de `C` y las copia en su lugar de la matriz final.
4. Cada servidor multiplica sus filas (`C_local = A_local × B`), mide cuánto tardó y regresa
   el resultado con su nombre de host.
5. El cliente espera a los 4 hilos, imprime el tiempo de cada servidor y el total.
   Con `--verificar` además recalcula todo en una sola máquina, compara y muestra el speedup.

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

Uso: `./cliente N [--verificar] servidor1 servidor2 ... servidorK`

- `N`: tamaño de la matriz.
- `--verificar`: recalcula todo en el cliente, compara e imprime el speedup
  (no usar con `N` muy grande: tarda lo mismo que una sola máquina).
- Se puede usar cualquier cantidad de servidores.
- Con `N <= 10` se imprimen A, B y C.

**Matriz pequeña (se imprime todo)**

```bash
docker exec -it rpc-matdist-cliente ./cliente 4 --verificar servidor1 servidor2 servidor3 servidor4
```
```
servidor1 (servidor1): filas 0 a 0, calculo 0.000 s, total con red 0.004 s
...
Matriz C:
  28   31   62   15
 122  104  106   71
 161  125  116  100
  62   55  112   42
Verificacion contra version local: CORRECTO
```

**Speedup (N = 1500)**

```bash
docker exec -it rpc-matdist-cliente ./cliente 1500 --verificar servidor1 servidor2 servidor3 servidor4
```
```
servidor1 (servidor1): filas 0 a 374, calculo 0.519 s, total con red 0.564 s
servidor2 (servidor2): filas 375 a 749, calculo 0.536 s, total con red 0.570 s
servidor3 (servidor3): filas 750 a 1124, calculo 0.527 s, total con red 0.569 s
servidor4 (servidor4): filas 1125 a 1499, calculo 0.543 s, total con red 0.586 s
Tiempo total (envio + calculo + recoleccion): 0.587 s
Verificacion contra version local: CORRECTO
Tiempo local (1 maquina): 2.288 s  ->  speedup: 3.90x
```

**Tamaño de la práctica (N = 10,000)** — tarda unos 2-3 minutos

```bash
docker exec -it rpc-matdist-cliente ./cliente 10000 servidor1 servidor2 servidor3 servidor4
```
```
servidor1 (servidor1): filas 0 a 2499, calculo 137.366 s, total con red 139.085 s
servidor2 (servidor2): filas 2500 a 4999, calculo 138.389 s, total con red 140.094 s
servidor3 (servidor3): filas 5000 a 7499, calculo 137.254 s, total con red 138.991 s
servidor4 (servidor4): filas 7500 a 9999, calculo 138.100 s, total con red 139.741 s
Tiempo total (envio + calculo + recoleccion): 140.097 s
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
