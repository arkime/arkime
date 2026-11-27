# Use Debian 13 slim as the base image
FROM debian:13-slim

ENV DEBIAN_FRONTEND=noninteractive

LABEL org.opencontainers.image.source="https://github.com/arkime/arkime"
LABEL org.opencontainers.image.description="Arkime 6 dev builds, see https://arkime.com/docker for suggested tags"

# OS tools layer. This does not depend on the Arkime build, so buildx can reuse it
# across builds when a cache is configured. ca-certificates is only a Recommends of
# curl, so it must be listed when recommends are skipped.
RUN sed -i "s/main$/main contrib/g" /etc/apt/sources.list.d/debian.sources && \
    apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates curl procps iproute2 geoipupdate tshark && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Arkime layer. ARGs are declared here so changing them only invalidates this layer.
# The build and release workflows always pass both; the defaults keep a plain
# `docker build -f release/Dockerfile .` working.
# Recommends are kept for this install: libwww-perl only recommends
# liblwp-protocol-https-perl, which db.pl needs for https.
ARG ARKIME_TAG=v6.0.0
ARG ARKIME_BUILD=6.0.0-1

RUN arch=$(arch | sed s/aarch64/arm64/ | sed s/x86_64/amd64/) && \
    echo "tag: ${ARKIME_TAG} build: ${ARKIME_BUILD} arch:${arch}" && \
    apt-get update && \
    curl -sSfLO https://github.com/arkime/arkime/releases/download/${ARKIME_TAG}/arkime_${ARKIME_BUILD}.debian13_${arch}.deb && \
    apt-get install -y ./arkime*.deb && \
    mv /opt/arkime/etc /opt/arkime/samples && \
    mkdir /opt/arkime/etc && \
    rm arkime*.deb && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY release/docker.sh /opt/arkime/bin/docker.sh

# Define entrypoint or command, e.g., starting Arkime or a shell
CMD ["/bin/bash"]
