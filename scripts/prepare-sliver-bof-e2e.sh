#!/usr/bin/env bash

set -euo pipefail

# Keep the planner's BOF build on the package workflow's reviewed toolchain.
readonly -a packages=(
  'binutils-mingw-w64-x86-64=2.41.90.20240122-1ubuntu1+11.4'
  'gcc-mingw-w64-base=13.2.0-6ubuntu1+26.1'
  'gcc-mingw-w64-x86-64-win32-runtime=13.2.0-6ubuntu1+26.1'
  'gcc-mingw-w64-x86-64-win32=13.2.0-6ubuntu1+26.1'
  'mingw-w64-common=11.0.1-3build1'
  'mingw-w64-x86-64-dev=11.0.1-3build1'
  'make=4.3-4.1build2'
)
sudo apt-get update
sudo apt-get install --yes --no-install-recommends "${packages[@]}"
sudo update-alternatives --set \
  x86_64-w64-mingw32-gcc \
  /usr/bin/x86_64-w64-mingw32-gcc-win32
test "$(x86_64-w64-mingw32-gcc -dumpmachine)" = x86_64-w64-mingw32
test "$(x86_64-w64-mingw32-gcc -dumpfullversion)" = 13-win32
test "$(x86_64-w64-mingw32-gcc -v 2>&1 | sed -n 's/^[[:space:]]*Thread model: //p')" = win32
for spec in "${packages[@]}"; do
  package="${spec%%=*}"
  expected="${spec#*=}"
  test "$(dpkg-query --show --showformat='${Version}' "$package")" = "$expected"
done

archive="$RUNNER_TEMP/boflink-v0.6.3-x86_64-unknown-linux-gnu.tar.gz"
curl --fail --location --proto '=https' --tlsv1.2 \
  --output "$archive" \
  "https://github.com/MEhrn00/boflink/releases/download/v0.6.3/$(basename "$archive")"
printf '%s  %s\n' \
  '9e4e88185ceee61cfc959dfd572ed22619301b76db47a357ebc9b766364f9c00' \
  "$archive" | sha256sum --check --strict
tar -xzf "$archive" -C "$RUNNER_TEMP"
install -D -m 0755 \
  "$RUNNER_TEMP/boflink-v0.6.3-x86_64-unknown-linux-gnu/boflink" \
  "$RUNNER_TEMP/bin/boflink"

export PATH="$RUNNER_TEMP/bin:$PATH"
make -C cat
make -C beaconinfo
