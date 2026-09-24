# Imagen de un nodo MPI: OpenMPI + mpi4py + ssh + hola_mpi.py.
# Todos los nodos usan la misma imagen, asi comparten la llave ssh
# y el script queda en la misma ruta en todas las "maquinas".
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        openmpi-bin python3 python-is-python3 python3-mpi4py \
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

# en contenedores no hay que forzar interfaz, pero se evita el aviso de memoria compartida
RUN printf "btl_vader_single_copy_mechanism = none\n" >> /etc/openmpi/openmpi-mca-params.conf

WORKDIR /home/mpi
COPY hola_mpi.py hola_mpi_mensaje.py hosts.txt ./
RUN chown -R mpi:mpi /home/mpi

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-e"]
