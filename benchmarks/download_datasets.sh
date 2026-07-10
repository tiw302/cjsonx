#!/bin/bash
# =========================================================================
# file: download_datasets.sh
# description: utility script to fetch standard json benchmarking datasets
#              (twitter.json, canada.json, citm_catalog.json) from official repos.
# =========================================================================
set -e

# get the directory of the script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p "$DIR/datasets"
cd "$DIR/datasets"

echo "Downloading twitter.json..."
curl -f -L -O https://raw.githubusercontent.com/simdjson/simdjson-data/master/jsonexamples/twitter.json

echo "Downloading canada.json..."
curl -f -L -O https://raw.githubusercontent.com/miloyip/nativejson-benchmark/master/data/canada.json

echo "Downloading citm_catalog.json..."
curl -f -L -O https://raw.githubusercontent.com/simdjson/simdjson-data/master/jsonexamples/citm_catalog.json

echo "Done."
