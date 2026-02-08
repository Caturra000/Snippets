#!/bin/bash
LINKER="$1"
as -o main.o main.s
as -o weak_impl.o weak_impl.s
as -o strong_impl.o strong_impl.s
"$LINKER" output main.o weak_impl.o strong_impl.o
chmod +x output
./output
EXIT_CODE=$?
if [ $EXIT_CODE -eq 222 ]; then
    echo "OK: strong symbol (222) overrides weak symbol (111)"
    exit 0
else
    echo "FAIL: expected 222 (strong), got $EXIT_CODE"
    exit 1
fi
