#!/bin/bash

# Compile the reference counting test

echo "Compiling reference counting test..."

gcc -o test_refcount test_refcount.c \
   ../src/utils/memory/ref_counter.c \
   ../src/utils/memory/ref_json.c \
   ../src/json.c \
   -I../include -Wall

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "Running test..."
    ./test_refcount
else
    echo "Compilation failed!"
fi