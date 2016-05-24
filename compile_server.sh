#!/bin/bash

echo "Compiling mdachunk"
cd server/labcomputer/mdachunk/src
qmake
make $1 -j 8
EXIT_CODE=$?
cd ../../..
if [[ $EXIT_CODE -ne 0 ]]; then
	false
fi
