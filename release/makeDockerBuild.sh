#!/bin/sh
VER=0.20.2
docker image build build7 --tag andywick/moloch-build-7:$VER
docker image build build6 --tag andywick/moloch-build-6:$VER
docker image build build14 --tag andywick/moloch-build-14:$VER
docker image build build16 --tag andywick/moloch-build-16:$VER
docker image build build16 --tag andywick/moloch-build-18:$VER

docker push andywick/moloch-build-7:$VER
docker push andywick/moloch-build-6:$VER
docker push andywick/moloch-build-14:$VER
docker push andywick/moloch-build-16:$VER
docker push andywick/moloch-build-18:$VER
