#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o func.o func.s
"$LINKER" output main.o func.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 42 ]; then
    echo "OK: multi-object linking works"
    exit 0
else
    echo "FAIL: expected 42, got $EXIT_CODE"
    exit 1
fi
