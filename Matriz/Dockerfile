# Multiplicacion de matrices con RPC (version de 2 servidores).
# Se compilan 2 variantes con los .h/.c ya generados que estan en el repo:
#   principal -> archivos de esta carpeta (N=10, arreglos de 100)
#   pruebas   -> carpeta Pruebas         (N=100, arreglos de 10000)
FROM debian:bookworm-slim

# netbase trae /etc/services y /etc/protocols, sin eso rpcbind no abre el puerto 111
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev libtirpc-dev rpcsvc-proto rpcbind netbase \
        iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /matriz
COPY *.c *.h ./principal/
COPY Pruebas/*.c Pruebas/*.h ./pruebas/

RUN for v in principal pruebas; do \
      cd /matriz/$v && \
      gcc -o servidor1 servidor1.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc && \
      gcc -o servidor2 servidor2.c matriz2_svc.c matriz2_xdr.c -I/usr/include/tirpc -ltirpc && \
      gcc -o cliente1 cliente1.c matriz_clnt.c matriz2_clnt.c matriz_xdr.c matriz2_xdr.c -I/usr/include/tirpc -ltirpc \
      || exit 1; \
    done

# servidor: primero rpcbind y luego servidor1 o servidor2 de la variante elegida
# (stdbuf -oL: para que los printf del servidor salgan en docker logs)
CMD ["sh", "-c", "rpcbind && exec stdbuf -oL ./$VARIANTE/servidor$SERVIDOR"]
