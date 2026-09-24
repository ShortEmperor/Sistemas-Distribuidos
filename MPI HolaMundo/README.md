# MPI Hola Mundo

Primera práctica de MPI (Python + mpi4py + OpenMPI): lanzar procesos en **2 computadoras**
con `mpirun` y mandar un mensaje entre ellas.

## Cómo funciona

1. `mpirun` lee el **hostfile** (`hosts.txt`) para saber en qué máquinas lanzar procesos y
   cuántos en cada una (`slots`).
2. Entra por **ssh** (sin contraseña) a cada máquina y arranca ahí el programa. Por eso el
   script tiene que estar **en la misma ruta en todas las máquinas**.
3. Todos los procesos forman parte del comunicador `MPI.COMM_WORLD`:
   - `rank`: número del proceso (0, 1, ...).
   - `size`: cuántos procesos hay en total.
4. Hay dos programas:
   - **`hola_mpi.py`**: cada proceso imprime `Hola Mundo desde el proceso <rank> de <size>`.
     No hay comunicación entre procesos.
   - **`hola_mpi_mensaje.py`**: el proceso 0 le **manda** un saludo a cada proceso
     (`comm.send`), cada uno lo **recibe** (`comm.recv`) y le **responde** al proceso 0.

```
      nodo1                                  nodo2
 ┌──────────────┐   ssh (lanza el proceso)  ┌──────────────┐
 │ mpirun       │ ────────────────────────► │              │
 │ proceso 0    │   send "Hola Mundo..."    │ proceso 1    │
 │              │ ────────────────────────► │ recv         │
 │ recv         │ ◄──────────────────────── │ send "Hola   │
 │              │    "Hola de vuelta..."    │  de vuelta"  │
 └──────────────┘                           └──────────────┘
```

## Diseño

**Archivos**

| Archivo | Qué es |
|---------|--------|
| `Codigo` | Archivo original: código y comandos `mpirun` juntos (se deja como evidencia) |
| `hola_mpi.py` | El código de `Codigo` separado (sin cambios) para poder ejecutarlo |
| `hola_mpi_mensaje.py` | Versión que sí manda mensajes entre procesos |
| `hosts.txt` | Hostfile: `nodo1` y `nodo2`, 1 proceso por nodo |
| `Librerias` | Notas de instalación en máquinas reales (Fedora) |
| `Dockerfile`, `docker-compose.yml` | Entorno Docker |

**`hola_mpi_mensaje.py`**

- Proceso 0: `send` a cada proceso `1..size-1` (tag 1) y luego `recv` de cada uno (tag 2).
- Demás procesos: `recv` del proceso 0, imprimen el mensaje y le responden con `send`.
- Cada mensaje incluye el hostname, para ver que realmente viene de la otra máquina.
- Si se ejecuta con un solo proceso, avisa que se necesitan al menos 2.

**Docker**

- 2 contenedores (`nodo1`, `nodo2`) con la misma imagen: `debian:bookworm-slim` + `openmpi-bin`
  + `python3-mpi4py` + servidor ssh.
- **ssh sin contraseña**: la llave se genera al construir la imagen, así los dos nodos la
  comparten y confían entre sí (equivale a `ssh-copy-id` en las máquinas reales).
- Se usa el usuario **`mpi`**: `mpirun` no deja correr como `root`.
- `python-is-python3`: para que funcione `python` (como en Fedora) además de `python3`.
- `hosts.txt` con **`slots=1`**: así `-np 2` pone un proceso en cada máquina.

## Ejecutar con Docker Compose

Desde esta carpeta:

```bash
docker compose up -d --build
docker compose down
```

| Contenedor | Rol |
|------------|-----|
| `mpi-hola-nodo1` | nodo 1 (desde aquí se lanza `mpirun`) |
| `mpi-hola-nodo2` | nodo 2 |

Los comandos siempre se ejecutan como usuario `mpi` (`-u mpi`).

## Pruebas

**Mandar un mensaje entre las 2 máquinas**

```bash
docker exec -it -u mpi mpi-hola-nodo1 mpirun -np 2 --hostfile hosts.txt python3 hola_mpi_mensaje.py
```
```
Proceso 1 en nodo2 recibio: 'Hola Mundo desde el proceso 0 en nodo1'
Proceso 0 en nodo1 recibio: 'Hola de vuelta desde el proceso 1 en nodo2'
```

**Comprobar que son 2 máquinas distintas**

```bash
docker exec -it -u mpi mpi-hola-nodo1 mpirun -np 2 --hostfile hosts.txt hostname
```
```
nodo1
nodo2
```

**Los comandos originales de `Codigo`**

```bash
# 4 procesos en la misma máquina
docker exec -it -u mpi mpi-hola-nodo1 mpirun -np 4 python hola_mpi.py

# 2 procesos, uno en cada máquina
docker exec -it -u mpi mpi-hola-nodo1 mpirun -np 2 --hostfile hosts.txt python3 hola_mpi.py

# forzando la interfaz de red
docker exec -it -u mpi mpi-hola-nodo1 mpirun --mca btl_tcp_if_include eth0 -np 2 --hostfile hosts.txt python3 hola_mpi.py

# ver en qué nodo queda cada proceso
docker exec -it -u mpi mpi-hola-nodo1 mpirun --display-map -np 2 --hostfile hosts.txt python3 hola_mpi.py
```
```
Hola Mundo desde el proceso 0 de 2
Hola Mundo desde el proceso 1 de 2
```

**Interfaz `lo` con mensajes** (se queda colgado a propósito, ver errores; `Ctrl+C` para salir)

```bash
docker exec -it -u mpi mpi-hola-nodo1 mpirun --mca btl_tcp_if_include lo -np 2 --hostfile hosts.txt python3 hola_mpi_mensaje.py
```

**Entrar a un nodo**

```bash
docker exec -it -u mpi mpi-hola-nodo1 bash
```

## Errores encontrados y correcciones

| Problema | Evidencia | Corrección |
|----------|-----------|------------|
| `Codigo` tenía el código y los comandos juntos, no se podía ejecutar | — | Se separó el código en `hola_mpi.py` (`Codigo` se deja igual) |
| `hosts.txt` no estaba en el repo | — | Se agregó |
| Con `slots=2`, `-np 2` metía **los 2 procesos en nodo1**: no eran 2 máquinas | `--display-map`: `nodo1 ... Num procs: 2` | `slots=1` por nodo |
| `hola_mpi.py` no manda ningún mensaje, solo imprime su rango | — | Se agregó `hola_mpi_mensaje.py` |
| Con `btl_tcp_if_include lo` funciona `hola_mpi.py` solo porque no hay mensajes; con mensajes se queda colgado (`lo` = localhost, no llega a la otra máquina) | `Open MPI accepted a TCP connection ... cannot find a corresponding process entry` | Usar la interfaz de red real (`eth0`) |
| En `Librerias` faltaba `:` en el `PATH` (`$HOME/bin//usr/lib64/...`) | `mpirun` no se encuentra | `$HOME/bin:/usr/lib64/openmpi/bin` |

## En máquinas reales (sin Docker)

Ver `Librerias` para la instalación completa (Fedora): paquetes, hostname, `/etc/hosts`, ssh y
firewall.

```bash
sudo dnf install openmpi openmpi-devel python3-mpi4py-openmpi -y
export PATH="$HOME/.local/bin:$HOME/bin:/usr/lib64/openmpi/bin:$PATH"

# hosts.txt con los nombres de las máquinas y los scripts en la misma ruta en todas
mpirun -np 2 --hostfile hosts.txt python3 hola_mpi_mensaje.py
```
