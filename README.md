# position_distributor

This project implements a distributor of positions among strategies.

Each strategy connects to the the distributor (server) via WebSocket.
They should send their own position when there is an update for some symbol and they will get broadcasts from all other strategies.

## Build Instructions

Boost library is required (`sudo dnf install boost-devel` on Fedora or `sudo apt-get install libboost-all-dev` on Ubuntu)

```
cmake -S . -B build
cmake --build build
```

## Test Instructions

```
cd build
bash ../test/test.sh
```

Output files will be located in `build/out`.
