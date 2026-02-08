#!/bin/bash
LINKER="$1"
as -o main.o main.s
if "$LINKER" output main.o 2>&1; then
    echo "FAIL: linker should have failed"
    exit 1
else
    echo "OK: linker correctly detected undefined symbol"
    exit 0
fi
