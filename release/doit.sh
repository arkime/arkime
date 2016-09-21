#!/bin/sh
vagrant up --no-provision
sleep 1

for i in ubuntu-14.04 ubuntu-16.04 centos-6 centos-7; do
    echo $i
    sleep 1
    sh -c "vagrant provision $i > $i.out 2>&1 || echo \"Error Occurred: $i\"" &
done
wait
