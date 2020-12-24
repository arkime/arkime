#!/bin/sh
export VER=3.0.0-4
#docker images -a | grep "arkime-build" | awk '{print $3}' | xargs docker rmi

docker image build build7 --tag andywick/arkime-build-7:$VER
docker image build build8 --tag andywick/arkime-build-8:$VER
docker image build build16 --tag andywick/arkime-build-16:$VER
docker image build build18 --tag andywick/arkime-build-18:$VER
docker image build build20 --tag andywick/arkime-build-20:$VER
docker image build buildArch --tag andywick/arkime-build-arch:$VER

exit 0

docker push andywick/arkime-build-7:$VER
docker push andywick/arkime-build-8:$VER
docker push andywick/arkime-build-16:$VER
docker push andywick/arkime-build-18:$VER
docker push andywick/arkime-build-20:$VER
docker push andywick/arkime-build-arch:$VER
