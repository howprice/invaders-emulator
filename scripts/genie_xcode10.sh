#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
pushd $SCRIPT_DIR > /dev/null
cd ..  # up into repo root

tools/bin/macosx/genie xcode10

popd > /dev/null