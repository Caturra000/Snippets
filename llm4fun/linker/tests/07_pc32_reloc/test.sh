#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o target.o target.s
"$LINKER" output main.o target.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 33 ]; then
    echo "OK: PC32 relocation works"
    exit 0
else
    echo "FAIL: expected 33, got $EXIT_CODE"
    exit 1
fi
