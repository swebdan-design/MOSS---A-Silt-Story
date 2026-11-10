#!/bin/sh
# Download the pinned GBDK-2020 release into .toolchain/gbdk (Linux x86_64).
# Verifies the archive's SHA-256 before extracting. Needs curl, tar, sha256sum.
set -eu
VERSION=4.3.0
SHA256=5032e4e4edcef3f0ed232c895bbd211973963329304ff2c030ea34d0b2d964a4
URL="https://github.com/gbdk-2020/gbdk-2020/releases/download/$VERSION/gbdk-linux64.tar.gz"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/.toolchain"
cd "$ROOT/.toolchain"
if [ -x gbdk/bin/lcc ]; then echo "GBDK already present: $ROOT/.toolchain/gbdk"; exit 0; fi
curl -sSL -o gbdk.tar.gz "$URL"
echo "$SHA256  gbdk.tar.gz" | sha256sum -c -
tar xzf gbdk.tar.gz
rm gbdk.tar.gz
echo "Installed GBDK-2020 $VERSION in $ROOT/.toolchain/gbdk"
