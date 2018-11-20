#!/bin/bash

# re-create these directories after installation so they are not part of the package manifest
CREATEDIRS="/data/<%= name %>/logs /data/<%= name %>/raw"
for CREATEDIR in ${CREATEDIRS}; do
    if [ ! -e ${CREATEDIR} ]; then
        mkdir -m 0700 -p ${CREATEDIR} && \
        chown nobody ${CREATEDIR}
    fi
done

echo "READ /data/<%= name %>/README.txt and RUN /data/<%= name %>/bin/Configure"
