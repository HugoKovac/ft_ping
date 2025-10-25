FROM debian:wheezy

ENV DEBIAN_FRONTEND=noninteractive

RUN echo "deb http://archive.debian.org/debian wheezy main contrib non-free" > /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian-security wheezy/updates main contrib non-free" >> /etc/apt/sources.list && \
    echo "Acquire::Check-Valid-Until false;" > /etc/apt/apt.conf.d/99no-check-valid && \
    echo "Acquire::AllowInsecureRepositories true;" > /etc/apt/apt.conf.d/99insecure && \
    echo "APT::Get::AllowUnauthenticated true;" >> /etc/apt/apt.conf.d/99insecure && \
    apt-get -o Acquire::Check-Valid-Until=false update

RUN apt-get install -y --force-yes \
    build-essential autoconf automake libtool pkg-config texinfo \
    wget git curl ca-certificates perl python python3

WORKDIR /opt
RUN wget https://ftp.gnu.org/gnu/inetutils/inetutils-2.0.tar.gz && \
    tar -xzf inetutils-2.0.tar.gz && \
    cd inetutils-2.0 && \
    ./configure --prefix=/usr && \
    make -j$(nproc) && \
    make install && \
    cd .. && rm -rf inetutils-2.0*



