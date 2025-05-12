#!/bin/bash
set -e

mkdir -p obj/database obj/api obj/js obj/transaction obj/tools obj/utils/memory bin

# Compile the reorganized files
for file in src/database/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/database/$(basename $file .c).o
done

for file in src/api/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/api/$(basename $file .c).o
done

for file in src/js/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/js/$(basename $file .c).o
done

for file in src/transaction/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/transaction/$(basename $file .c).o
done

for file in src/utils/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/utils/$(basename $file .c).o
done

for file in src/utils/memory/*.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/utils/memory/$(basename $file .c).o
done

# Compile the core files
for file in src/api.c src/cors.c src/json.c src/jwt.c src/query_language.c src/quickjs_mock.c src/rbac.c src/rbac_refcount.c src/server.c src/ssl.c; do
  echo "Compiling $file..."
  gcc -Wall -I./include -DTOOLS_BUILD -c $file -o obj/$(basename $file .c).o
done

# Compile our fixed main file
echo "Compiling src/main_fixed.c..."
gcc -Wall -I./include -DTOOLS_BUILD -c src/main_fixed.c -o obj/main_fixed.o

# Gather all object files
OBJECTS=$(find obj -name "*.o")

# Build the server
echo "Linking server..."
gcc $OBJECTS -o bin/jsondb -pthread -lm -L/opt/qjs/lib/quickjs -lquickjs

echo "Build completed!"