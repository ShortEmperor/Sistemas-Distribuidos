# Matriz RPC distribuida: una imagen para los 4 servidores y el cliente.
FROM debian:bookworm-slim

# netbase trae /etc/services y /etc/protocols, sin eso rpcbind no abre el puerto 111
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase \
        iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /matriz
COPY matriz.x servidor.c cliente.c ./

# rpcgen -M: stubs seguros para hilos (el cliente llama a los servidores en paralelo)
RUN rpcgen -M matriz.x \
 && gcc -O2 -o servidor servidor.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc \
 && gcc -O2 -o cliente cliente.c matriz_clnt.c matriz_xdr.c -I/usr/include/tirpc -ltirpc -lpthread

# servidor: primero rpcbind (stdbuf -oL: para que los printf salgan en docker logs)
CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./servidor"]
