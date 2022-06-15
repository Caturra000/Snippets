#!/bin/sh
g++ client.cpp -DFLUENT_FLAG_DLOG_ENABLE -o client -pthread &
g++ server.cpp -DFLUENT_FLAG_DLOG_ENABLE -o server -pthread &

wait
