#!/bin/sh
export VER=5.0.0-1
#docker images -a | grep "arkime-build" | awk '{print $3}' | xargs docker rmi

echo "ARKIME DOCKER 7"
docker image build build7 --no-cache=true --tag andywick/arkime-build-7:$VER

echo "ARKIME DOCKER 8"
docker image build build8 --no-cache=true --tag andywick/arkime-build-8:$VER

echo "ARKIME DOCKER 9"
docker image build build9 --no-cache=true --tag andywick/arkime-build-9:$VER

echo "ARKIME DOCKER 18"
docker image build build18 --no-cache=true --tag andywick/arkime-build-18:$VER

echo "ARKIME DOCKER 20"
docker image build build20 --no-cache=true --tag andywick/arkime-build-20:$VER

echo "ARKIME DOCKER 22"
docker image build build22 --no-cache=true --tag andywick/arkime-build-22:$VER

echo "ARKIME DOCKER Arch"
docker image build buildArch --no-cache=true --tag andywick/arkime-build-arch:$VER

echo "ARKIME DOCKER Al2023"
docker image build buildAl2023 --no-cache=true --tag andywick/arkime-build-al2023:$VER


exit 0

docker push andywick/arkime-build-7:$VER
docker push andywick/arkime-build-8:$VER
docker push andywick/arkime-build-9:$VER
docker push andywick/arkime-build-18:$VER
docker push andywick/arkime-build-20:$VER
docker push andywick/arkime-build-22:$VER
docker push andywick/arkime-build-arch:$VER
docker push andywick/arkime-build-al2023:$VER
