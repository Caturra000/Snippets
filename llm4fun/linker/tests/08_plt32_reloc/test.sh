#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o external.o external.s
"$LINKER" output main.o external.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 44 ]; then
    echo "OK: PLT32 relocation works"
    exit 0
else
    echo "FAIL: expected 44, got $EXIT_CODE"
    exit 1
fi
