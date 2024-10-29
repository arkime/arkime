# Use Debian 12 slim as the base image
FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

ARG ARKIME_VERSION=5.4.0

# Install necessary dependencies and download the .deb package
RUN arch=$(arch | sed s/aarch64/arm64/ | sed s/x86_64/amd64/) && \
    apt update && \
    apt install -y wget && \
    wget https://github.com/arkime/arkime/releases/download/v${ARKIME_VERSION}/arkime_${ARKIME_VERSION}-1.debian12_${arch}.deb && \
    apt install -y ./arkime*.deb && \
    mv /opt/arkime/etc /opt/arkime/samples && \
    mkdir /opt/arkime/etc && \
    rm arkime*.deb && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Define entrypoint or command, e.g., starting Arkime or a shell
CMD ["/bin/bash"]
