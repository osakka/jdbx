#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

/* 
 * Debug macros for conditional debugging output
 * Set DEBUG_MEMORY to 1 to enable memory management debug prints,
 * or to 0 to disable them in production builds.
 */
#define DEBUG_MEMORY 0

#if DEBUG_MEMORY
    #define DEBUG_PRINT(fmt, ...) printf("DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) /* No output in release builds */
#endif

#endif /* DEBUG_H */