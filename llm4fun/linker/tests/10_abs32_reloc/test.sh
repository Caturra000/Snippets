#!/bin/bash
LINKER="$1"
as -o main.o main.s
"$LINKER" output main.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 88 ]; then
    echo "OK: 32-bit address/value works"
    exit 0
else
    echo "FAIL: expected 88, got $EXIT_CODE"
    exit 1
fi
