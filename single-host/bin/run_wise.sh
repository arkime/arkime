#!/bin/sh
# Add to /etc/inittab something like
# v1:2345:respawn:/data/moloch/bin/run_wise.sh

TDIR=_TDIR_

cd ${TDIR}/wiseService
/bin/rm -f ${TDIR}/logs/wise.log.old
/bin/mv ${TDIR}/logs/wise.log ${TDIR}/logs/wise.log.old
export NODE_ENV=production

# The wiseService will look for wiseService.ini in _TDIR_/etc by default
exec ${TDIR}/bin/node-wise wiseService.js -c ${TDIR}/etc/wiseService.ini > ${TDIR}/logs/wise.log 2>&1
