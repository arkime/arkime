#!/bin/bash

# Entrypoint for arkime.
# Set ulimit memory lock to unlimited.
# Take $ARIKME_CONFIG file and envsubst it (via https://github.com/a8m/envsubst to support defaults)
# stripping the extra file extention before writing it:
# envsubst > /opt/arkime/etc/config.ini < /opt/arkime/etc/config.ini.envsubst

set -e

if [ "$ARIKME_SET_ULIMIT" == "true" ];
then
    echo "Setting ulimit -l unlimited"
    ulimit -l unlimited
fi

ARIKME_CONFIG=${ARIKME_CONFIG:-/opt/arkime/etc/config.ini.envsubst}

if [ -f "$ARIKME_CONFIG" ];
then
    # shellcheck disable=SC2001
    envsubst > "$(echo "$ARIKME_CONFIG" | sed -e "s/\.[^\.]*$//")" < "$ARIKME_CONFIG"
fi

exec "$@"
