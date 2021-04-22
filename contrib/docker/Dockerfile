## TODO: Pin versions in apt get install
## TODO: make the final container non-root?

#### BUILD STAGE ####
FROM ubuntu:xenial as build
LABEL maintainer="Ivan Rimac <ivan@33barrage.com>"

# add bitcoin repo so we can install all dependencies
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C70EF1F0305A1ADB9986DBD8D46F45428842CE5E && \
    echo "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu xenial main" > /etc/apt/sources.list.d/bitcoin.list

# install dependencies
RUN apt-get update \
	&& apt-get install -y --no-install-recommends \ 
	cmake \
	build-essential \
	libssl-dev \
	ibdb4.8-dev \
	libdb4.8++-dev \
	libboost-all-dev \
	libqrencode-dev \
	libevent-dev \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*


# change workdir to dedicated workspace
WORKDIR /stealth

# fetch the compressed source of stealth
ADD https://github.com/StealthSend/Stealth/archive/qpos-3.0.tar.gz .

# unpack the source of stealth
RUN tar xvzf ./*qpos-3.0*.tar.gz

# clean up after
RUN mv ./Stealth-*/* .
RUN rm -Rf ./Stealth-* ./*qpos-3.0*.tar.gz

# change the workdir to source of stealth
WORKDIR /stealth/src

# build stealth from source
RUN make -f makefile.unix 

# change to main workspace
WORKDIR /stealth

# copy built binary to local bin directory
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

# move repo binaries to local bin directory
COPY ./scripts /usr/local/bin
COPY docker-entrypoint.sh /usr/local/bin/

# make them executable
RUN chmod -R +x /usr/local/bin

#### FINAL STAGE ####
FROM phusion/baseimage:0.10.2

ARG USER_ID
ARG GROUP_ID

ENV HOME /stealth

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}

# add our user and group first to make sure their IDs get assigned consistently, regardless of whatever dependencies get added
RUN groupadd -g ${GROUP_ID} stealth \
	&& useradd -u ${USER_ID} -g stealth -s /bin/bash -m -d /stealth stealth

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		libevent-dev \
		ca-certificates \
		wget \
		p7zip-full \
	&& apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# change to main workspace
WORKDIR /stealth

# copy binaries from build stage
COPY --from=build /usr/local/bin /bin

# create a volume for external configuration and blockchain data
VOLUME ["/stealth"]

# change ownership of the working directory
RUN chown -R stealth:stealth /stealth

# expose p2p and rpc ports
EXPOSE 4437 4438 46502 46503

# leave entrypoint for extra commands to run
ENTRYPOINT ["docker-entrypoint.sh"]

# main command to run when container starts
CMD ["stealth_oneshot"]
