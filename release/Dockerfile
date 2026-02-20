# Use Debian 13 slim as the base image
FROM debian:13-slim

ENV DEBIAN_FRONTEND=noninteractive

ARG ARKIME_TAG=v6.0.0
ARG ARKIME_BUILD=6.0.0-1

LABEL org.opencontainers.image.source="https://github.com/arkime/arkime"
LABEL org.opencontainers.image.description="Arkime 6 dev builds, see https://arkime.com/docker for suggested tags"

# Install necessary dependencies and download the .deb package
RUN arch=$(arch | sed s/aarch64/arm64/ | sed s/x86_64/amd64/) && \
    echo "tag: ${ARKIME_TAG} build: ${ARKIME_BUILD} arch:${arch}" && \
    sed -i "s/main$/main contrib/g" /etc/apt/sources.list.d/debian.sources && \
    apt update && \
    apt install -y curl wget procps iproute2 geoipupdate && \
    wget -nv https://github.com/arkime/arkime/releases/download/${ARKIME_TAG}/arkime_${ARKIME_BUILD}.debian13_${arch}.deb && \
    apt install -y ./arkime*.deb && \
    mv /opt/arkime/etc /opt/arkime/samples && \
    mkdir /opt/arkime/etc && \
    rm arkime*.deb && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY release/docker.sh /opt/arkime/bin/docker.sh

# Define entrypoint or command, e.g., starting Arkime or a shell
CMD ["/bin/bash"]
