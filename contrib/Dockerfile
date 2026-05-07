FROM ubuntu:24.04 AS build

LABEL maintainer="Hemp0x Developers"

ENV DEBIAN_FRONTEND=noninteractive
ARG BUILD_JOBS=4

RUN apt-get update && \
	apt-get install -y --no-install-recommends \
	build-essential autoconf automake libtool pkg-config bsdmainutils \
	curl python3 gawk ca-certificates binutils file git patch && \
	rm -rf /var/lib/apt/lists/*

COPY . /home/hemp0x/build/hemp0x-core-next/
WORKDIR /home/hemp0x/build/hemp0x-core-next

RUN contrib/build-hemp0x-core.sh --target linux --with-tx --jobs "${BUILD_JOBS}"

FROM ubuntu:24.04 AS final

LABEL maintainer="Hemp0x Developers"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
	apt-get install -y --no-install-recommends ca-certificates libstdc++6 && \
	rm -rf /var/lib/apt/lists/*

EXPOSE 8766/tcp
EXPOSE 42069/tcp

RUN useradd -ms /bin/bash hemp0x && \
	mkdir /var/lib/hemp0x && \
	chown hemp0x:hemp0x /var/lib/hemp0x && \
	ln -s /var/lib/hemp0x /home/hemp0x/.hemp0x && \
	chown -h hemp0x:hemp0x /home/hemp0x/.hemp0x

VOLUME /var/lib/hemp0x

COPY --from=build /home/hemp0x/build/hemp0x-core-next/src/hemp0xd /usr/local/bin/hemp0xd
COPY --from=build /home/hemp0x/build/hemp0x-core-next/src/hemp0x-cli /usr/local/bin/hemp0x-cli
COPY --from=build /home/hemp0x/build/hemp0x-core-next/src/hemp0x-tx /usr/local/bin/hemp0x-tx

WORKDIR /home/hemp0x
USER hemp0x

CMD ["/usr/local/bin/hemp0xd", "-datadir=/var/lib/hemp0x", "-printtoconsole", "-onlynet=ipv4"]
