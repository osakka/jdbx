#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Testing index headers...
");
    #if __has_include("index/btree_disk.h")
        printf("✓ btree_disk.h found
");
    #else
        printf("✗ btree_disk.h NOT found
");
    #endif
    return 0;
}
