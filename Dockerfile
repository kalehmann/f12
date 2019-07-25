FROM debian

RUN apt-get update && apt-get install --no-install-recommends -y \
	autoconf \
	automake \
	build-essential \
	ca-certificates \
	curl \
	git \
	libtool \
	libtool-bin \
	pkg-config \
	valgrind \
	&& rm -rf /var/lib/apt/lists/*

RUN curl -L https://github.com/libcheck/check/releases/download/0.12.0/check-0.12.0.tar.gz | tar zx \
	&& cd check-0.12.0 \
	&& ./configure \
	&& make \
	&& make check \
	&& make install \
	&& cd .. \
	&& rm -rf check-0.12.0

RUN git clone https://github.com/bats-core/bats-core.git \
	&& cd bats-core \
	&& ./install.sh /usr/local \
	&& cd .. \
	&& rm -rf bats-core


