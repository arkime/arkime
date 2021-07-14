#!/bin/bash

# Entrypoint for arkime.
# Set ulimit memory lock to unlimited.
# Take $ARIKME_CONFIG file and envsubst it (via https://github.com/a8m/envsubst to support defaults)
# stripping the extra file extention before writing it:
# cat /opt/arkime/etc/config.ini.envsubst | envsubst > /opt/arkime/etc/config.ini

set -e

echo "Setting ulimit -l unlimited"
ulimit -l unlimited

ARIKME_CONFIG=${ARIKME_CONFIG:-/opt/arkime/etc/config.ini.envsubst}

if [ -f $ARIKME_CONFIG ];
then
    cat $ARIKME_CONFIG | envsubst > $(echo "$ARIKME_CONFIG" | sed -e "s/\.[^\.]*$//") 
fi

exec "$@"
