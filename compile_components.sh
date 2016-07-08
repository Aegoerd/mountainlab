#!/bin/bash

if [ -z "$1" ]; then
    echo "usage:"
    echo "./compile_components.sh default"
    echo "./compile_components.sh mountainview mountainbrowser"
    echo "example components: mdachunk mdaconvert mountainbrowser mountainoverlook mountainprocess mountainsort mountainview"
    exit 0
fi

if [ $1 == "default" ]
then
qmake
else
qmake "COMPONENTS = $@"
fi

make -j 8
EXIT_CODE=$?
if [[ $EXIT_CODE -ne 0 ]]; then
	false
fi
