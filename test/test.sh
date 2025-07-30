
#!/bin/bash

# Run this script from build directory

# Paths to binaries
SERVER_BIN="./position_distributor"
CLIENT_BIN="test/test_client"

# Output files
OUTPUT_DIR="out"
mkdir -p $OUTPUT_DIR
SERVER_OUT="$OUTPUT_DIR/server_output.log"
BINANCE_OUT="$OUTPUT_DIR/client_binance_output.log"
HUOBI_OUT="$OUTPUT_DIR/client_huobi_output.log"
KUCOIN_OUT="$OUTPUT_DIR/client_kucoin_output.log"

# Cleanup function to kill all background jobs
cleanup() {
    echo "\nStopping all processes..."
    jobs -p | xargs -r kill
    exit 0
}

trap cleanup SIGINT

# Start server (output to both terminal and file)
stdbuf -oL $SERVER_BIN | tee $SERVER_OUT &
SERVER_PID=$!

# Give server a moment to start
sleep 1

# Start clients (output to files)
stdbuf -oL $CLIENT_BIN BN > $BINANCE_OUT 2>&1 &
BINANCE_PID=$!
stdbuf -oL $CLIENT_BIN HB > $HUOBI_OUT 2>&1 &
HUOBI_PID=$!
stdbuf -oL $CLIENT_BIN KC > $KUCOIN_OUT 2>&1 &
KUCOIN_PID=$!

echo "Server PID: $SERVER_PID"
echo "BINANCE Client PID: $BINANCE_PID"
echo "HUOBI Client PID: $HUOBI_PID"
echo "KUCOIN Client PID: $KUCOIN_PID"

# Wait for background jobs
wait
