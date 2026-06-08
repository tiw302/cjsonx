#!/bin/bash
set -e

# get the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p "$DIR/datasets"
cd "$DIR/datasets"

echo "Downloading twitter.json..."
curl -L -O https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json

echo "Downloading canada.json..."
curl -L -O https://raw.githubusercontent.com/miloyip/nativejson-benchmark/master/data/canada.json

echo "Downloading citm_catalog.json..."
curl -L -O https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/citm_catalog.json

echo "Done."
