#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o weak.o weak.s
"$LINKER" output main.o weak.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 77 ]; then
    echo "OK: weak symbol works"
    exit 0
else
    echo "FAIL: expected 77, got $EXIT_CODE"
    exit 1
fi
