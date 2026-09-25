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
- **Solo `nodo1` construye la imagen** (`build` + `pull_policy: build`); `nodo2` usa esa misma
  imagen (`pull_policy: never`), para que los dos tengan exactamente la misma llave ssh.
- Se usa el usuario **`mpi`**: `mpirun` no deja correr como `root`.
- `python-is-python3`: para que funcione `python` (como en Fedora) además de `python3`.
- `hosts.txt` con **`slots=1`**: así `-np 2` pone un proceso en cada máquina.

## Código explicado

### `hola_mpi.py`

```python
from mpi4py import MPI          # mpi4py: MPI para Python (usa OpenMPI por debajo);
                                # al importarlo se inicializa MPI (MPI_Init)
comm = MPI.COMM_WORLD           # comunicador con TODOS los procesos que lanzó mpirun
rank = comm.Get_rank()          # número de este proceso: 0, 1, 2, ...
size = comm.Get_size()          # cuántos procesos hay en total (el -np de mpirun)

print(f"Hola Mundo desde el proceso {rank} de {size}")
```

`mpirun -np 2 ...` ejecuta este mismo script **2 veces** (en paralelo, en las máquinas del
hostfile); cada copia obtiene un `rank` distinto. El orden en que salen los mensajes no está
garantizado: cada proceso imprime cuando le toca.

### `hola_mpi_mensaje.py`

| Parte | Código | Qué hace |
|-------|--------|----------|
| Inicio | `comm`, `rank`, `size` igual que arriba; `host = socket.gethostname()` | Además guarda el nombre de la máquina |
| Validación | `if size < 2` | Con un solo proceso no hay a quién mandarle; solo avisa |
| Proceso 0 | `for destino in range(1, size): comm.send(texto, dest=destino, tag=1)` | Manda un saludo a cada uno de los demás procesos. `tag=1` marca el tipo de mensaje ("saludo") |
| Proceso 0 | `for origen in range(1, size): respuesta = comm.recv(source=origen, tag=2)` | Espera la respuesta de cada proceso, en orden (1, 2, ...), e imprime lo que recibió |
| Demás procesos | `mensaje = comm.recv(source=0, tag=1)` | Se **bloquea** hasta que llega el saludo del proceso 0 |
| Demás procesos | `print(...)`, `comm.send(texto, dest=0, tag=2)` | Imprime el saludo y le responde al proceso 0 con `tag=2` ("respuesta") |

- `comm.send` / `comm.recv` (en minúsculas) mandan **cualquier objeto de Python**: mpi4py lo
  serializa con `pickle`. (Las versiones con mayúscula, `Send`/`Recv`, mandan arreglos
  directamente y son más rápidas.)
- `flush=True` en los `print` hace que el texto salga en cuanto se imprime (si no, `mpirun`
  podría mostrarlo hasta el final).
- Las etiquetas (`tag`) sirven para que un `recv` solo acepte el tipo de mensaje que espera.

### `Codigo`

Archivo original: las primeras 11 líneas son `hola_mpi.py`, y después hay comandos de
`mpirun` como notas:

| Comando | Qué hace |
|---------|----------|
| `mpirun -np 4 python hola_mpi.py` | 4 procesos en la máquina local (sin hostfile) |
| `mpirun -np 2 --hostfile hosts.txt python3 hola_mpi.py` | 2 procesos repartidos según `hosts.txt` |
| `mpirun --mca btl_tcp_if_include eth0 ...` | Obliga a MPI a comunicarse solo por la interfaz `eth0` (cuando la máquina tiene varias redes y MPI elige la equivocada) |
| `mpirun --mca btl_tcp_if_include lo ...` | Lo mismo con `lo` (localhost): solo sirve si todo está en la misma máquina |
| `mpirun --display-map ...` | Antes de ejecutar, muestra en qué nodo queda cada proceso |

### `Librerias`

Notas para preparar las máquinas reales (Fedora):

| Comando | Para qué |
|---------|----------|
| `dnf install openmpi openmpi-devel python3-mpi4py-openmpi` | OpenMPI, sus herramientas de compilación y mpi4py compilado contra OpenMPI |
| `hostnamectl set-hostname nodo1` | Nombre de la máquina (el que se usa en el hostfile) |
| `nano /etc/hosts` | Asociar IP ↔ nombre de cada nodo, para que `nodo2` se resuelva |
| `firewall-cmd --add-service=ssh --permanent` / `--reload` | Abrir ssh en el firewall (mpirun entra por ssh) |
| `ufw disable` / `enable` | Apagar/prender el firewall (MPI abre puertos aleatorios para comunicarse) |
| `systemctl status sshd` / `start sshd` | Revisar y arrancar el servidor ssh |
| `export PATH=...:/usr/lib64/openmpi/bin:$PATH` | En Fedora `mpirun` no está en el `PATH` por defecto |
| `ip addr add 10.10.10.2/24 dev eth0 label eth0:1` | Agregar una IP extra a la interfaz (para poner los nodos en la misma subred) |

### `hosts.txt`

```
nodo1 slots=1
nodo2 slots=1
```

Una línea por máquina; `slots` = cuántos procesos puede recibir. `mpirun` llena los slots en
orden, así que con `slots=1` el proceso 0 va a `nodo1` y el 1 a `nodo2`.

### `Dockerfile`

| Instrucción | Qué hace |
|-------------|----------|
| `FROM debian:bookworm-slim` | Sistema base |
| `apt-get install openmpi-bin python3 python-is-python3 python3-mpi4py openssh-server openssh-client ...` | OpenMPI, Python con mpi4py, el comando `python` (como en Fedora), servidor y cliente ssh |
| `useradd -m -s /bin/bash mpi` | Usuario normal: `mpirun` se niega a correr como `root` |
| `ssh-keygen -A` + `StrictHostKeyChecking no` en `/etc/ssh/ssh_config` | Llaves del servidor ssh; no preguntar "¿confías en este host?" la primera vez que se conecta a otro nodo |
| `ssh-keygen -t ed25519 ...` + `cp id_ed25519.pub authorized_keys` (como `mpi`) | Crea la llave del usuario y la autoriza. Como todos los nodos tienen la misma imagen, todos tienen la misma llave y confían entre sí |
| `btl_vader_single_copy_mechanism = none` en `openmpi-mca-params.conf` | Evita un aviso de OpenMPI dentro de contenedores (no se permite el mecanismo de copia entre procesos que usa por defecto) |
| `COPY hola_mpi.py hola_mpi_mensaje.py hosts.txt ./` | Scripts y hostfile en `/home/mpi` (misma ruta en los dos nodos) |
| `CMD ["/usr/sbin/sshd", "-D", "-e"]` | El contenedor solo corre el servidor ssh (en primer plano) esperando a que `mpirun` entre |

### `docker-compose.yml`

- `nodo1`: construye la imagen (`build: .`, `pull_policy: build`) y la etiqueta como
  `mpi-holamundo-nodo:latest`.
- `nodo2`: usa esa misma imagen (`pull_policy: never`: no intentar bajarla de Docker Hub) y
  arranca después de `nodo1` (`depends_on`).
- `hostname: nodo1` / `nodo2`: los nombres que usa `hosts.txt`.
- `shm_size: 256m`: memoria compartida para OpenMPI (el valor por defecto de Docker, 64 MB, es
  poco).

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
