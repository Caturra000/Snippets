#!/bin/bash
LINKER="$1"
as -o main.o main.s
"$LINKER" output main.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 99 ]; then
    echo "OK: BSS section works (zero-initialized, writable)"
    exit 0
else
    echo "FAIL: expected 99, got $EXIT_CODE"
    exit 1
fi
