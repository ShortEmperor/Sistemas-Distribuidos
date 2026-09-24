# Calculadora distribuida: una imagen para las 4 computadoras y el cliente.
FROM debian:bookworm-slim

# netbase trae /etc/services y /etc/protocols, sin eso rpcbind no abre el puerto 111
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase \
        iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /rpc
COPY operaciones.x servidor.c cliente.c ./

# generar los stubs con rpcgen y compilar
RUN rpcgen -h operaciones.x -o operaciones.h \
 && rpcgen -c operaciones.x -o operaciones_xdr.c \
 && rpcgen -l operaciones.x -o operaciones_clnt.c \
 && rpcgen -m operaciones.x -o operaciones_svc.c \
 && gcc -o servidor servidor.c operaciones_svc.c operaciones_xdr.c -I/usr/include/tirpc -ltirpc \
 && gcc -o cliente cliente.c operaciones_clnt.c operaciones_xdr.c -I/usr/include/tirpc -ltirpc

# cada servidor: primero rpcbind y luego su operacion (OPERACION viene del compose)
CMD ["sh", "-c", "rpcbind && exec ./servidor $OPERACION"]
