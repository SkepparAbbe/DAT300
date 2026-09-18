FROM ubuntu:20.04

# Prevent interactive apt prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install software dependencies from the paper:
# GSL, TBB, pthreads (via build-essential), GOMP (via gcc), CMake, Git
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgsl-dev \
    libtbb-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory inside the container
WORKDIR /workspace

# Copy the entire artifact into the container
COPY . /workspace/

# Build the main IP.LSH.DBSCAN project
RUN cd /workspace/code/IP.LSH.DBSCAN && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make -j$(nproc)

# Ensure the pre-compiled computeAccuracy_fast binary is executable.
# (It only links to standard C/C++ libraries and works on x86_64 Ubuntu 20.04.)
RUN chmod +x /workspace/code/IP.LSH.DBSCAN/benchmarks/computeAccuracy_fast

# Add build directory to PATH for convenience
ENV PATH="/workspace/code/IP.LSH.DBSCAN/build:/workspace/code/IP.LSH.DBSCAN/benchmarks:${PATH}"

# Default to a bash shell
CMD ["/bin/bash"]
