#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
pushd $SCRIPT_DIR > /dev/null
cd ..  # up into repo root

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    tools/bin/linux/genie clean
elif [[ "$OSTYPE" == "linux-gnueabihf" ]]; then
    tools/bin/raspberry-pi/genie clean
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    tools/bin/macosx/genie clean
fi

popd > /dev/null
