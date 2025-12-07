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

# Create +MANIFEST file - minimal format for pkg create
# Note: Don't include "prefix" in the manifest when using -r, pkg will handle it
cat > "$PKGDIR/+MANIFEST" <<EOF
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
  "licenselogic": "single",
  "licenses": ["Apache20"]
}
EOF

# Create +POST_INSTALL script if afterinstall.sh exists
#if [ -f "release/afterinstall.sh" ]; then
#    cp "release/afterinstall.sh" "$PKGDIR/+POST_INSTALL"
#fi

# Generate plist by walking the install directory, excluding logs and raw directories
PLIST_FILE="$PKGDIR/plist"
(
    cd "$INSTALL_DIR"
    # Files: strip leading ./ and convert to paths relative to prefix
    find . -type f ! -path "*/logs/*" ! -path "*/raw/*" -print | sed 's|^\./||'
    # Directories: strip leading ./, remove trailing slashes, add @dir prefix, sort in reverse for proper removal order
    find . -type d ! -name . ! -path "*/logs*" ! -path "*/raw*" -print | sed 's|^\./||' | sed 's|/$||' | sort -r | awk '{print "@dir " $0}'
) | sort -u > "$PLIST_FILE"

# Create the package using pkg create
pkg create \
    -m "$PKGDIR" \
    -r "${INSTALL_DIR}" \
    -p "$PLIST_FILE" \
    -o . \
    arkime

# Rename the package to match the naming convention
PKG_FILE="arkime-${ARKIME_VERSION}.pkg"
if [ -f "$PKG_FILE" ]; then
    mv "$PKG_FILE" "arkime_${ARKIME_VERSION}-${ITERATION}.${ARCH_ONLY}.pkg"
    ls -l "arkime_${ARKIME_VERSION}-${ITERATION}.${ARCH_ONLY}.pkg"
fi
