#!/bin/sh
# Add to /etc/inittab something like 
# v1:2345:respawn:_TDIR_/bin/run_viewer.sh

TDIR=_TDIR_

cd ${TDIR}/viewer
/bin/rm -f ${TDIR}/logs/viewer.log.old
/bin/mv ${TDIR}/logs/viewer.log ${TDIR}/logs/viewer.log.old
export NODE_ENV=production 
exec ${TDIR}/bin/node-viewer viewer.js -c ${TDIR}/etc/config.ini > ${TDIR}/logs/viewer.log 2>&1
