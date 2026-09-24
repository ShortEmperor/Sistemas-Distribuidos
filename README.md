# Sistemas Distribuidos

Prácticas de la materia de Sistemas Distribuidos: **RPC** (Sun RPC / rpcgen, en C) y **MPI**
(OpenMPI). Cada práctica está **dockerizada**: se levanta con un solo comando y se prueba con
`docker exec`, simulando cada computadora con un contenedor.

| Carpeta | Práctica | Lenguaje | Contenedores |
|---------|----------|----------|--------------|
| [`RPC/`](RPC/) | Calculadora RPC (las 4 operaciones en un servidor) | C | servidor + cliente |
| [`RPC/distribuido/`](RPC/distribuido/) | Calculadora RPC, **una operación por computadora** | C | 4 servidores + cliente |
| [`Matriz/`](Matriz/) | Matrices con RPC, versión de 2 servidores | C | 2 servidores + cliente |
| [`Matriz/distribuido/`](Matriz/distribuido/) | Matrices con RPC, **10,000 × 10,000 entre 4 computadoras** | C | 4 servidores + cliente |
| [`Practica2_matrices/`](Practica2_matrices/) | Matrices con RPC, plantilla de 4 servidores | C | 4 servidores + cliente |
| [`MPI HolaMundo/`](MPI%20HolaMundo/) | Hola Mundo con MPI y envío de mensajes entre 2 computadoras | Python | 2 nodos |
| [`MPI Matriz/`](MPI%20Matriz/) | Matrices con MPI, **10,000 × 10,000 entre 4 computadoras** | C | 4 nodos |

Cada carpeta tiene su `README.md` con cómo funciona, el diseño, cómo levantarla, cómo
probarla y los errores que se encontraron.

## Requisitos

- Docker con Docker Compose (Docker Desktop en Windows/Mac).
- Para las matrices de 10,000 × 10,000: unos **4 GB de RAM** libres para Docker.

## Uso general

```bash
cd <carpeta>
docker compose up -d --build     # construir y levantar
docker exec -it <contenedor> ... # probar (ver el README de la carpeta)
docker compose down              # apagar
```

Los nombres de los contenedores son distintos en cada práctica, así que se pueden tener varias
levantadas al mismo tiempo.
