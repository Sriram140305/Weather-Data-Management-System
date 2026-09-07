#!/bin/sh
set -e

echo "Compiler:"
g++ --version | head -n 1

if [ -z "$WEATHER_API_KEY" ]; then
    echo "ERROR: WEATHER_API_KEY is not set."
    echo 'Run: export WEATHER_API_KEY="YOUR_REAL_WEATHERAPI_KEY"'
    exit 1
fi

echo "Building..."
g++ -std=c++17 -pthread *.cpp -o weather -lcurl

echo "Build successful."
echo "Starting program..."
./weather
