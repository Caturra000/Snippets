#!/bin/sh
g++ client.cpp -o client -pthread -I../../fluent &
g++ server.cpp -o server -pthread -I../../fluent &

wait
