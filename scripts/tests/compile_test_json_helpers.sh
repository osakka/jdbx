#!/bin/bash

# Compile the test program
gcc -Wall -I../include -o test_json_helpers test_transaction_json_helpers.c ../src/server.c ../src/json.c -lm

# Make the binary executable
chmod +x test_json_helpers

# Run the test
./test_json_helpers