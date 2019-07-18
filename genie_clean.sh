#!/bin/bash
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    ./tools/bin/linux/genie clean
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./tools/bin/macosx/genie clean
fi
