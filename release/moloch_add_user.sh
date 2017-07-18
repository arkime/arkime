#!/bin/sh

# Check the existance of a local copy of node and preference that local version
if command -v "BUILD_MOLOCH_INSTALL_DIR/bin/node" >/dev/null 2>&1; then
    "BUILD_MOLOCH_INSTALL_DIR/bin/node" "BUILD_MOLOCH_INSTALL_DIR/viewer/addUser.js" -c "BUILD_MOLOCH_INSTALL_DIR/etc/config.ini" "$@"
# Check if node is installed globally    
elif command -v node >/dev/null 2>&1; then
    node "BUILD_MOLOCH_INSTALL_DIR/viewer/addUser.js" -c "BUILD_MOLOCH_INSTALL_DIR/etc/config.ini" "$@"
# Good luck - maybe you have node elsewhere
else
    echo "Node is required to be installed to run this command"
fi
