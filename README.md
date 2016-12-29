# Distributed Shared Memory
Synchronizes shared memory buffers between multiple machines

## Requirements
 - Boost:
   - System
   - Thread
   - Interprocess
   - Asio
   - Program\_options

## Optional
 - Boost:
   - Log
   - Date\_Time
   - Python3

## Known to work under
 - Operating Systems:
   - OSX 10.11.5
   - Raspbian Jessie Lite
 - Compilers:
   - Clang 3.8
   - GCC 4.9.2 ARM hf
 - Boost >= 1.55
 - Python >= 3.4.2
 - CMake >= 3.0.2


## Build instructions for Ubuntu 14
### Install prerequisites
- `sudo add-apt-repository ppa:george-edison55/cmake-3.x`
- `sudo apt-get update`
- `sudo apt-get install cmake`
- `sudo apt-get install libboost1.55-all-dev`

### Install DSM
- `git clone git@gitlab.com:sdrobotics101/DistributedSharedMemory.git`
- `cd DistributedSharedMemory`
- `git submodule update --init`
- `cmake CMakeLists.txt`
- `make`