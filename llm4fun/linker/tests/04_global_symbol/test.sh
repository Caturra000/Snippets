#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o data.o data.s
"$LINKER" output main.o data.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 55 ]; then
    echo "OK: global symbol works"
    exit 0
else
    echo "FAIL: expected 55, got $EXIT_CODE"
    exit 1
fi
