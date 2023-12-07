#!/bin/bash

if [ ! -d build ]; then
	mkdir -p build
fi

echo building...
gcc -xc -g -Og -pthread -lm -o./build/main00 ./src/main00.c

echo done...

