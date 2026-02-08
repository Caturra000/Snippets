#!/bin/bash
LINKER="$1"
as -o file1.o file1.s
as -o file2.o file2.s
if "$LINKER" output file1.o file2.o 2>&1; then
    echo "FAIL: linker should have failed on duplicate symbols"
    exit 1
else
    echo "OK: linker correctly detected duplicate symbol"
    exit 0
fi
