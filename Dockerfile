FROM debian:12-slim

RUN apt-get update \
    	&& apt-get upgrade -y \
    	&& apt-get install --no-install-recommends -y \
	autoconf \
	automake \
	autopoint \
	build-essential \
	ca-certificates \
	curl \
	gettext \
	git \
	indent \
	lcov \
	libtool \
	libtool-bin \
	nasm \
	pkg-config \
	valgrind \
	xxd \
	&& rm -rf /var/lib/apt/lists/*

RUN curl -L https://github.com/libcheck/check/releases/download/0.15.2/check-0.15.2.tar.gz | tar zx \
	&& cd check-0.15.2 \
	&& ./configure \
	&& make \
	&& make check \
	&& make install \
	&& cd .. \
	&& rm -rf check-0.15.2

RUN git clone https://github.com/bats-core/bats-core.git \
	&& cd bats-core \
	&& ./install.sh /usr/local \
	&& cd .. \
	&& rm -rf bats-core

RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/usr-local-lib.conf \
	&& ldconfig

