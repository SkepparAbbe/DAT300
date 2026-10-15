#!/bin/bash

echo "Rebuilding IP.LSH.DBSCAN"

rm -rf ../build
mkdir ../build

cd ../build
cmake ..
make
cd -
echo "Rebuild complete"

echo "Profiling implementation..."
./profile_lshdbscan_flamegraph.sh
echo "Run complete"
