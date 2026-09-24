Colocar en todas las maquinas el comando

sudo systemctl start rpcbind

Cambiar por las ip en 
Char servidores[4]


Realizar ejecutable
opcion1:
gcc -o servidor servidor.c matriz_svc.c matriz_xdr.c -lnsl

gcc -o cliente cliente.c matriz_clnt.c matriz_xdr.c -lnsl

opcion2:
gcc -o servidor servidor.c matriz_svc.c matriz_xdr.c -I/usr/include/tirpc -ltirpc


gcc -o cliente clientem.c matriz_clnt.c matriz_xdr.c -I/usr/include/tirpc -ltirpc


ejecutar el cliente
./cliente


servidor 
./servidor


Agregar el servidor.c 
system("hostname"); 
para que aparfezca que si están usando dos maquinas

