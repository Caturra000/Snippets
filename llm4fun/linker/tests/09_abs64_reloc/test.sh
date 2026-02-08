#!/bin/bash
LINKER="$1"
as -o main.o main.s
"$LINKER" output main.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 66 ]; then
    echo "OK: R_X86_64_64 relocation works"
    exit 0
else
    echo "FAIL: expected 66, got $EXIT_CODE"
    exit 1
fi
