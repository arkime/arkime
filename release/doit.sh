#!/bin/sh
# https://dzone.com/articles/parallel-provisioning-speeding

#!/bin/sh
 
# concurrency is hard, let's have a beer
 
MAX_PROCS=4
 
parallel_provision() {
    while read box; do
        echo "Provisioning '$box'. Output will be in: $box.out.txt" 1>&2
        echo $box
    done | xargs -P $MAX_PROCS -I"BOXNAME" \
        sh -c 'vagrant provision BOXNAME >BOXNAME.out.txt 2>&1 || echo "Error Occurred: BOXNAME"'
}
 
## -- main -- ##
 
# start boxes sequentially to avoid vbox explosions
vagrant up --no-provision

sleep 1
 
# but run provision tasks in parallel
cat <<EOF | parallel_provision
ubuntu-14.04
ubuntu-16.04
centos-6
centos-7
EOF
