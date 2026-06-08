#!/bin/bash
set -e

# get the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p "$DIR/third_party"
cd "$DIR/third_party"

echo "Downloading cJSON..."
curl -L -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
curl -L -O https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h

echo "Downloading jsmn..."
curl -L -O https://raw.githubusercontent.com/zserge/jsmn/master/jsmn.h

echo "Downloading yyjson..."
curl -L -O https://raw.githubusercontent.com/ibireme/yyjson/master/src/yyjson.c
curl -L -O https://raw.githubusercontent.com/ibireme/yyjson/master/src/yyjson.h

echo "Done downloading third party libraries."
