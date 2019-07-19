#!/bin/bash
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    ./tools/bin/linux/genie gmake
elif [[ "$OSTYPE" == "linux-gnueabihf" ]]; then
    ./tools/bin/raspberry-pi/genie gmake
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./tools/bin/macosx/genie gmake
fi
