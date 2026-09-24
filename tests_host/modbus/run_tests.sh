#!/usr/bin/env bash
# Test host cho lop Modbus cua ak-base-kit-pio. Chay: bash tests_host/modbus/run_tests.sh
set -e
cd "$(dirname "$0")"
N=../../sources/application/networks
CFLAGS="-std=c99 -Wall -Wextra -g -DTASK_MBSLAVE_EN -I$N/nanomodbus -I$N/mb_port -I."
mkdir -p build
build() { gcc $CFLAGS -o "build/$1.exe" "$1.c" fake_link.c "$N/nanomodbus/nanomodbus.c" "${@:2}"; }
build test_slave_regs "$N/mb_port/mb_slave_regs.c" "$N/mb_port/mb_ota.c"
build test_ota "$N/mb_port/mb_slave_regs.c" "$N/mb_port/mb_ota.c"
for t in build/*.exe; do "$t"; done
