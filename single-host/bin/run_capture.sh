#!/bin/sh
# Add to /etc/inittab something like 
# m1:2345:respawn:_TDIR_/bin/run_capture.sh

TDIR=_TDIR_

cd ${TDIR}/bin
/bin/rm -f ${TDIR}/logs/capture.log.old
/bin/mv ${TDIR}/logs/capture.log ${TDIR}/logs/capture.log.old

${TDIR}/bin/moloch-capture -c ${TDIR}/etc/config.ini > ${TDIR}/logs/capture.log 2>&1
#screen -d -RR CAPTURE ${TDIR}/bin/capture.cmd
