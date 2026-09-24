# Imagen para la calculadora RPC: compila servidor y cliente desde el codigo
# (no se usan los binarios subidos al repo).
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase \
        iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /rpc
COPY *.c *.h *.x ./

# Makefile.calculadora (el que genera rpcgen) compila las plantillas vacias
# calculadora_server.c/calculadora_client.c, por eso se compila a mano:
RUN gcc -o servidor calculadora_svc.c calculadora_srv.c calculadora_xdr.c -I/usr/include/tirpc -ltirpc \
 && gcc -o cliente cliente.c calculadora_clnt.c calculadora_xdr.c -I/usr/include/tirpc -ltirpc

# servidor: primero rpcbind (sudo systemctl start rpcbind en las maquinas reales)
CMD ["sh", "-c", "rpcbind && exec ./servidor"]
