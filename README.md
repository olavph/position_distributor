# position_distributor

This project implements a distributor of positions among strategies.

Each strategy connects to the the distributor (server) via WebSocket.
They should send their own position when there is an update for some symbol and they will get broadcasts from all other strategies.

## Requirements

### Correctness

Correctness is guranteed by the strategies sending every update on their symbols.
With WebSocket (over TCP), messages are guaranteed to be delivered to the server, which in turn distributes them to all other strategies.

### Order preservation

Websocket gurantees messages are delivered in the order they are sent. Boost [strands](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/core/strands.html) guarantee the async socket writes to happen sequentially, for each client and for the server.

### Resilience

If a strategy connection drops, it should re-send all open positions on re-connection. This is not simulated in the test.

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
