FROM ubuntu:xenial
MAINTAINER Ivan Rimac <ivan@33barrage.com>

ARG USER_ID
ARG GROUP_ID

ENV HOME /stealth

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}

# add our user and group first to make sure their IDs get assigned consistently, regardless of whatever dependencies get added
RUN groupadd -g ${GROUP_ID} stealth \
	&& useradd -u ${USER_ID} -g stealth -s /bin/bash -m -d /stealth stealth

RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C70EF1F0305A1ADB9986DBD8D46F45428842CE5E && \
    echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin.list

RUN apt-get update \
	&& apt-get install -y build-essential \
	&& apt-get install -y libssl-dev \
	&& apt-get install -y libdb4.8-dev \
	&& apt-get install -y libdb4.8++-dev \
	&& apt-get install -y libboost-all-dev \
	&& apt-get install -y libqrencode-dev \
	&& apt-get install -y libevent-dev \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*


WORKDIR /stealth
ADD https://github.com/StealthSend/Stealth/archive/master.tar.gz .
RUN tar xvzf *.tar.gz
RUN mv Stealth-*/* .
RUN rm -Rf Stealth-* *.tar.gz

WORKDIR /stealth/src
RUN make -f makefile.unix
WORKDIR /stealth
RUN cp /stealth/src/StealthCoind /usr/local/bin

# grab gosu for easy step-down from root
ENV GOSU_VERSION 1.11
RUN set -x \
	&& apt-get update && apt-get install -y --no-install-recommends \
		ca-certificates \
		wget \
	&& wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture)" \
	&& wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture).asc" \
	&& export GNUPGHOME="$(mktemp -d)" \
	&& gpg --keyserver ha.pool.sks-keyservers.net --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4 \
	&& gpg --batch --verify /usr/local/bin/gosu.asc /usr/local/bin/gosu \
	&& rm -r "$GNUPGHOME" /usr/local/bin/gosu.asc \
	&& chmod +x /usr/local/bin/gosu \
	&& gosu nobody true \
	&& apt-get purge -y \
		ca-certificates \
		wget \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD ./bin /usr/local/bin

VOLUME ["/stealth"]

EXPOSE 4437 4438 46502 46503

WORKDIR /stealth

COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod -R +x /usr/local/bin
RUN ls -all /usr/local/bin
ENTRYPOINT ["docker-entrypoint.sh"]

CMD ["stealth_oneshot"]