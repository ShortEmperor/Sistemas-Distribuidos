# Practica2_matrices: 4 servidores con el mismo programa RPC + 1 cliente.
FROM debian:bookworm-slim

# netbase trae /etc/services y /etc/protocols, sin eso rpcbind no abre el puerto 111
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase \
        iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /matriz
COPY matriz.x servidor.c clientem.c ./

# en esta carpeta no estan los archivos generados, se generan con rpcgen
RUN rpcgen matriz.x \
 && gcc -o servidor servidor.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc \
 && gcc -o cliente clientem.c matriz_clnt.c matriz_xdr.c -I/usr/include/tirpc -ltirpc

# servidor: primero rpcbind (stdbuf -oL: para que los printf salgan en docker logs)
CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./servidor"]
