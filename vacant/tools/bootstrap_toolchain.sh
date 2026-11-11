#!/bin/sh
# Download the pinned GBDK-2020 release into .toolchain/gbdk (Linux x86_64),
# verifying its SHA-256 before extracting. Needs curl, tar and sha256sum.
# On macOS/Windows download the same GBDK-2020 release for your platform and
# run `make GBDK_HOME=/path/to/gbdk`.
set -eu
VERSION=4.5.0
SHA256=d7857a5f6d135ee4c249043ca26aad9f2ec8ab5d4106d97720d404114f42605c
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
