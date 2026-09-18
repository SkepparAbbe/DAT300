# SRRDBSCAN

This is the source code for SRRDBSCAN as developed for [the paper](https://openproceedings.org/2025/conf/edbt/paper-208.pdf): 

> Camilla Birch Okkels, Martin Aumüller, Viktor Bello Thomsen, Arthur Zimek:
High-dimensional density-based clustering using locality-sensitive hashing. EDBT 2025: 694-706


The code is available in `src`, an example program is provided in `apps`. A python wrapper is provided in `python/` and [python/test/test.py](python/test/test.py) gives a basic example.

The experimental evaluation uses the Python wrapper. The evaluation framework can be found at <https://github.com/CamillaOkkels/dbscan-benchmark>.

# Requirements

On a standard Ubuntu 22.04, 

```
apt update -y && apt install -y libtbb-dev python3-numpy python3-scipy python3-pip build-essential git
```

covers all requirements to build the code.


How to compile:
```bash
    mkdir build; cd build; cmake ..; make
```

Building the python package works as follows

```bash 
   python setup.py build
```

Input files are supposed to be HDF5 files.

All data processing is provided with the evaluation framework <https://github.com/CamillaOkkels/dbscan-benchmark>.


# Acknowledgements

The code is based on 

> Amir Keramatian, Vincenzo Gulisano, Marina Papatriantafilou, Philippas Tsigas: IP.LSH.DBSCAN: Integrated Parallel Density-Based Clustering Through Locality-Sensitive Hashing. Euro-Par 2022: 268-284.
