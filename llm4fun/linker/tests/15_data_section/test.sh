#!/bin/bash
LINKER="$1"
as -o main.o main.s
"$LINKER" output main.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 77 ]; then
    echo "OK: DATA section works (initialized, writable)"
    exit 0
else
    echo "FAIL: expected 77, got $EXIT_CODE"
    exit 1
fi
