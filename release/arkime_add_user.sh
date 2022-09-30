#!/bin/sh

if [ -f "BUILD_ARKIME_INSTALL_DIR/etc/config.ini" ]; then
  BUILD_ARKIME_INSTALL_DIR/bin/node BUILD_ARKIME_INSTALL_DIR/viewer/addUser.js -c BUILD_ARKIME_INSTALL_DIR/etc/config.ini "$@"
elif [ -f "BUILD_ARKIME_INSTALL_DIR/etc/cont3xt.ini" ]; then
  BUILD_ARKIME_INSTALL_DIR/bin/node BUILD_ARKIME_INSTALL_DIR/viewer/addUser.js -c BUILD_ARKIME_INSTALL_DIR/etc/cont3xt.ini "$@"
else
  echo "Couldn't find config.ini or cont3xt.ini file"
fi
