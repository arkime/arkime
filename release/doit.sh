#!/bin/sh
vagrant destroy -f
sleep 2
vagrant up --no-provision
sleep 2

echo "UP"

for i in ubuntu-14.04 ubuntu-16.04 centos-6 centos-7; do
    echo $i
    sh -c "vagrant provision $i > $i.out 2>&1 || echo \"Error Occurred: $i\"" &
    sleep 3
done
wait
