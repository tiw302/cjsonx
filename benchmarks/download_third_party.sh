#!/bin/bash
set -e

# get the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p "$DIR/third_party"
cd "$DIR/third_party"

echo "Downloading cJSON v1.7.18..."
curl -L -O https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.c
curl -L -O https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.h

echo "Downloading jsmn v1.1.0..."
curl -L -O https://raw.githubusercontent.com/zserge/jsmn/v1.1.0/jsmn.h

echo "Downloading yyjson 0.10.0..."
curl -L -O https://raw.githubusercontent.com/ibireme/yyjson/0.10.0/src/yyjson.c
curl -L -O https://raw.githubusercontent.com/ibireme/yyjson/0.10.0/src/yyjson.h

echo "Done downloading third party libraries."
