#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
pushd $SCRIPT_DIR > /dev/null
cd ..  # up into repo root

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    tools/bin/linux/genie gmake
elif [[ "$OSTYPE" == "linux-gnueabihf" ]]; then
    tools/bin/raspberry-pi/genie gmake
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    tools/bin/macosx/genie gmake
fi

popd > /dev/null
