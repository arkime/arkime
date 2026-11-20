#!/bin/sh
# Alpine apk creation script using apk mkpkg, needs apk-tools 3 and scanelf
# Usage: ./alpinepkg.sh <version> <iteration> <output_file> <install_dir>

set -e

if [ $# -ne 4 ]; then
    echo "Usage: $0 <version> <iteration> <output_file> <install_dir>"
    exit 1
fi

# apk versions only allow lowercase suffixes joined with _, so 7.0.0-GIT becomes 7.0.0_git
ARKIME_VERSION=$(echo "$1" | tr 'A-Z-' 'a-z_')
ITERATION=$2
OUTPUT=$3
INSTALL_DIR=$4

APKARCH=$(apk --print-arch)
case "$APKARCH" in
    x86_64) MACHINE=EM_X86_64 ;;
    aarch64) MACHINE=EM_AARCH64 ;;
    *) echo "Unsupported arch $APKARCH"; exit 1 ;;
esac

# Depend on the sonames the binaries need, like abuild does, so apk picks the providers.
# npm installs glibc and other arch prebuilds that are never loaded here, skip them.
SODEPS=$(scanelf -RBF '%a|%n|' "$INSTALL_DIR" | awk -F'|' -v m="$MACHINE" -v arch="$APKARCH" '
    $1 != m || $2 == "" || $2 ~ /libc\.so\.6|ld-linux/ {next}
    {
        n = split($2, libs, ",")
        for (i = 1; i <= n; i++) {
            if (libs[i] == "libc.so") print "so:libc.musl-" arch ".so.1"
            else print "so:" libs[i]
        }
    }' | sort -u | tr '\n' ' ')

ROOT=$(mktemp -d)
trap 'rm -rf $ROOT' EXIT

mkdir -p "$ROOT$INSTALL_DIR"
for f in "$INSTALL_DIR"/*; do
    case "$(basename "$f")" in
        logs|raw) continue ;;
    esac
    cp -a "$f" "$ROOT$INSTALL_DIR/"
done

apk mkpkg \
    -I name:arkime \
    -I "version:$ARKIME_VERSION-r$ITERATION" \
    -I "arch:$APKARCH" \
    -I "description:Arkime Full Packet System" \
    -I url:https://arkime.com \
    -I license:Apache-2.0 \
    -I "depends:bash curl ethtool perl perl-json perl-libwww perl-lwp-protocol-https $SODEPS" \
    -s "post-install:$(dirname "$0")/afterinstall.sh" \
    -F "$ROOT" \
    -o "$OUTPUT"
