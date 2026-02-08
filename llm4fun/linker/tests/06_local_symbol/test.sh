#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o other.o other.s
"$LINKER" output main.o other.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 11 ]; then
    echo "OK: local symbols don't conflict"
    exit 0
else
    echo "FAIL: expected 11, got $EXIT_CODE"
    exit 1
fi
