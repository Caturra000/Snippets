#!/bin/bash
LINKER="$1"
as -o file1.o file1.s
as -o file2.o file2.s
"$LINKER" output file1.o file2.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 30 ]; then
    echo "OK: section merge works (exit=30)"
    exit 0
else
    echo "FAIL: expected 30, got $EXIT_CODE"
    exit 1
fi
