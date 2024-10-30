# Use Debian 12 slim as the base image
FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

ARG ARKIME_TAG=v5.4.0
ARG ARKIME_BUILD=5.4.0-1

# Install necessary dependencies and download the .deb package
RUN arch=$(arch | sed s/aarch64/arm64/ | sed s/x86_64/amd64/) && \
    echo "tag: ${ARKIME_TAG} build: ${ARKIME_BUILD} arch:${arch}" && \
    apt update && \
    apt install -y wget procps && \
    wget -nv https://github.com/arkime/arkime/releases/download/${ARKIME_TAG}/arkime_${ARKIME_BUILD}.debian12_${arch}.deb && \
    apt install -y ./arkime*.deb && \
    mv /opt/arkime/etc /opt/arkime/samples && \
    mkdir /opt/arkime/etc && \
    rm arkime*.deb && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY release/docker.sh /opt/arkime/bin/docker.sh

# Define entrypoint or command, e.g., starting Arkime or a shell
CMD ["/bin/bash"]
