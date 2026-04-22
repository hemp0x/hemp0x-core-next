FROM amd64/ubuntu:20.04 AS base

#If you found this docker image helpful please donate HEMP to the maintainer
LABEL maintainer="RV9zdNeUTQUToZUcRp9uNF8gwH5LzDFtan"

EXPOSE 8766/tcp
EXPOSE 42069/tcp

ENV DEBIAN_FRONTEND=noninteractive

#Install runtime dependencies
RUN apt-get update && \
	apt-get install -y --no-install-recommends \
	bash wget net-tools libminiupnpc17 \
	libevent-2.1 libevent-pthreads-2.1 \
	libboost-system1.71 libboost-filesystem1.71 libboost-chrono1.71 \
	libboost-program-options1.71 libboost-thread1.71 \
	libzmq5 && \
	apt-get clean

FROM base AS build

#Install build dependencies
RUN apt-get update && \
	apt-get install -y --no-install-recommends \
	bash net-tools build-essential libtool autotools-dev automake \
	pkg-config libssl-dev libevent-dev bsdmainutils python3 \
	libboost-system1.71-dev libboost-filesystem1.71-dev libboost-chrono1.71-dev \
	libboost-program-options1.71-dev libboost-test1.71-dev libboost-thread1.71-dev \
	libzmq3-dev libminiupnpc-dev && \
	apt-get clean

#Copy source dir
COPY . /home/hemp0x/build/Hemp0x/
WORKDIR /home/hemp0x/build/Hemp0x

#build db4 from source
WORKDIR /home/hemp0x/build/Hemp0x/contrib
RUN ./install_db4.sh ../../

# build Hemp0xcore
WORKDIR /home/hemp0x/build/Hemp0x
RUN ./autogen.sh && ./configure --disable-tests BDB_LIBS="-L/home/hemp0x/build/db4/lib -ldb_cxx-4.8" BDB_CFLAGS="-I/home/hemp0x/build/db4/include" --without-gui  && make -j4

FROM base AS final

#Add our service account user
RUN useradd -ms /bin/bash hemp0x && \
	mkdir /var/lib/hemp0x && \
	chown hemp0x:hemp0x /var/lib/hemp0x && \
	ln -s /var/lib/hemp0x /home/hemp0x/.hemp0x && \
	chown -h hemp0x:hemp0x /home/hemp0x/.hemp0x

VOLUME /var/lib/hemp0x

#Copy the compiled binaries from the build
COPY --from=build /home/hemp0x/build/Hemp0x/src/hemp0xd /usr/local/bin/hemp0xd
COPY --from=build /home/hemp0x/build/Hemp0x/src/hemp0x-cli /usr/local/bin/hemp0x-cli

WORKDIR /home/hemp0x
USER hemp0x

CMD /usr/local/bin/hemp0xd -datadir=/var/lib/hemp0x -printtoconsole -onlynet=ipv4
