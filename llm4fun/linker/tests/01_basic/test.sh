#!/bin/bash
LINKER="$1"
as -o start.o start.s
"$LINKER" output start.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 42 ]; then
    echo "OK: exit code = 42"
    exit 0
else
    echo "FAIL: expected 42, got $EXIT_CODE"
    exit 1
fi
