#!/bin/bash

# Compile mountainsort/cpp
echo "Compiling mountainsort"
cd mountainsort/src
qmake
make -j 8
EXIT_CODE=$?
cd ../..
if [[ $EXIT_CODE -ne 0 ]]; then
	false
fi
