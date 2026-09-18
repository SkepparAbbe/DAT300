#! /bin/bash

epsilon=0.001;minPts=500;numberOfHashTables=5;numberOfHyperPlanesPerTable=2;files=" -f ../../datasets/geolife/geolife_small.txt -b ../../datasets/geolife/geolife_small.idx"

for numberOfThreads in 1 2 3 4 5 10 15 20 25 30 35 40 45 50 55 60 65 70
do    
    params=" -m ${minPts} -e ${epsilon} -t ${numberOfThreads} -M ${numberOfHyperPlanesPerTable} -L ${numberOfHashTables}"
    command="../build/LSHDBSCAN_exec"$files$params
    $command
done

mv accuracy.txt ./accuracy_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv benchmark.txt ./benchmark_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv memory_usage.log ./memory_usage_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.log
###################################################################################################
epsilon=0.001;minPts=500;numberOfHashTables=10;numberOfHyperPlanesPerTable=2;files=" -f ../../datasets/geolife/geolife_small.txt -b ../../datasets/geolife/geolife_small.idx"

for numberOfThreads in 1 2 3 4 5 10 15 20 25 30 35 40 45 50 55 60 65 70
do    
    params=" -m ${minPts} -e ${epsilon} -t ${numberOfThreads} -M ${numberOfHyperPlanesPerTable} -L ${numberOfHashTables}"
    command="../build/LSHDBSCAN_exec"$files$params
    $command
done

mv accuracy.txt ./accuracy_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv benchmark.txt ./benchmark_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv memory_usage.log ./memory_usage_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.log
###################################################################################################
epsilon=0.001;minPts=500;numberOfHashTables=20;numberOfHyperPlanesPerTable=2;files=" -f ../../datasets/geolife/geolife_small.txt -b ../../datasets/geolife/geolife_small.idx"

for numberOfThreads in 1 2 3 4 5 10 15 20 25 30 35 40 45 50 55 60 65 70
do    
    params=" -m ${minPts} -e ${epsilon} -t ${numberOfThreads} -M ${numberOfHyperPlanesPerTable} -L ${numberOfHashTables}"
    command="../build/LSHDBSCAN_exec"$files$params
    $command
done

mv accuracy.txt ./accuracy_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv benchmark.txt ./benchmark_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.txt
mv memory_usage.log ./memory_usage_L_${numberOfHashTables}_M_${numberOfHyperPlanesPerTable}.log
