#!/bin/sh
# FreeBSD package creation script using pkg create
# Usage: ./freebsdpkg.sh <version> <iteration> <arch> <install_dir>

set -e

if [ $# -ne 4 ]; then
    echo "Usage: $0 <version> <iteration> <arch> <install_dir>"
    exit 1
fi

ARKIME_VERSION=$1
ITERATION=$2
ARCH=$3
INSTALL_DIR=$4

# Normalize arch names from "freebsd14.x86_64" format to just "x86_64"
ARCH_ONLY=$(echo "$ARCH" | sed 's/^freebsd[0-9]*\.//')

# Normalize arch names for FreeBSD compatibility
case "$ARCH_ONLY" in
    amd64) PKG_ARCH="amd64" ;;
    x86_64) PKG_ARCH="amd64" ;;
    arm64) PKG_ARCH="aarch64" ;;
    aarch64) PKG_ARCH="aarch64" ;;
    *) PKG_ARCH="$ARCH_ONLY" ;;
esac

# Create temporary directory for package metadata
PKGDIR=$(mktemp -d)
trap "rm -rf $PKGDIR" EXIT

# Get FreeBSD version
FBSD_VERSION=$(uname -r | cut -d. -f1)

# Create +MANIFEST file with correct prefix
cat > "$PKGDIR/+MANIFEST" <<MANIFEST
{
  "name": "arkime",
  "version": "${ARKIME_VERSION}",
  "origin": "net-mgmt/arkime",
  "comment": "Arkime Full Packet System",
  "desc": "Arkime is a large scale, open source, indexed packet capture and search system.",
  "maintainer": "arkime@arkime.com",
  "www": "https://arkime.com",
  "abi": "FreeBSD:${FBSD_VERSION}:${PKG_ARCH}",
  "arch": "${PKG_ARCH}",
  "prefix": "/usr/local/share/arkime",
  "licenselogic": "single",
  "licenses": ["Apache20"]
}
MANIFEST

# Create +POST_INSTALL script if afterinstall.sh exists
#if [ -f "release/afterinstall.sh" ]; then
#    cp "release/afterinstall.sh" "$PKGDIR/+POST_INSTALL"
#fi

# Generate plist with absolute paths from filesystem root
# This way pkg create won't try to relativize or adjust them
PLIST_FILE="$PKGDIR/plist"
(
    cd "$INSTALL_DIR"
    # Files: use absolute paths
    find . -type f ! -path "*/logs/*" ! -path "*/raw/*" -print | while read f; do
        echo "${INSTALL_DIR}/${f#./}"
    done
    # Directories: use absolute paths with @dir prefix, sorted in reverse for proper cleanup
    find . -type d ! -name . ! -path "*/logs*" ! -path "*/raw*" -print | while read d; do
        echo "@dir ${INSTALL_DIR}/${d#./}"
    done | sort -r
) > "$PLIST_FILE"

# Create the package without -r flag, using absolute paths in plist
pkg create \
    -m "$PKGDIR" \
    -p "$PLIST_FILE" \
    -o . \
    arkime

# Rename the package to match the naming convention
PKG_FILE="arkime-${ARKIME_VERSION}.pkg"
if [ -f "$PKG_FILE" ]; then
    mv "$PKG_FILE" "arkime_${ARKIME_VERSION}-${ITERATION}.${ARCH_ONLY}.pkg"
    ls -l "arkime_${ARKIME_VERSION}-${ITERATION}.${ARCH_ONLY}.pkg"
fi
