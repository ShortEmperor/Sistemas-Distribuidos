# Imagen de un nodo MPI: OpenMPI + ssh + el programa ya compilado.
# Todos los nodos usan la misma imagen, asi comparten la llave ssh
# y el ejecutable queda en la misma ruta en todas las "maquinas".
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        openmpi-bin libopenmpi-dev gcc libc6-dev make \
        openssh-server openssh-client iproute2 iputils-ping procps \
    && rm -rf /var/lib/apt/lists/*

# usuario normal (mpirun no quiere correr como root)
RUN useradd -m -s /bin/bash mpi

# ssh sin contraseña entre nodos (misma llave en toda la imagen)
RUN mkdir -p /run/sshd && ssh-keygen -A \
    && printf "Host *\n    StrictHostKeyChecking no\n    UserKnownHostsFile /dev/null\n    LogLevel ERROR\n" >> /etc/ssh/ssh_config
USER mpi
RUN mkdir -p /home/mpi/.ssh && ssh-keygen -t ed25519 -N "" -f /home/mpi/.ssh/id_ed25519 \
    && cp /home/mpi/.ssh/id_ed25519.pub /home/mpi/.ssh/authorized_keys \
    && chmod 700 /home/mpi/.ssh && chmod 600 /home/mpi/.ssh/authorized_keys
USER root

# OpenMPI dentro de contenedores: usar solo la red del contenedor (eth0)
RUN printf "btl_tcp_if_include = eth0\noob_tcp_if_include = eth0\nbtl_vader_single_copy_mechanism = none\n" \
        >> /etc/openmpi/openmpi-mca-params.conf

WORKDIR /home/mpi
COPY matriz_mpi.c hosts ./
RUN mpicc -O2 -o matriz_mpi matriz_mpi.c && chown -R mpi:mpi /home/mpi

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-e"]
