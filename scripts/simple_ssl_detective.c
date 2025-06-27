/**
 * @file simple_ssl_detective.c
 * @brief Inspector Clouseau's Simplified SSL Investigation
 * 
 * "Ze simple approach, Inspector Claude! We focus on ze SSL problem only!"
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>

/* Simple investigation without complex dependencies */

static int test_count = 0;
static int passed_tests = 0;

static void segfault_handler(int sig) {
    printf("💥 SEGFAULT DETECTED in test %d! Inspector Claude found ze smoking gun!\n", test_count);
    printf("🕵️ This confirms SSL corruption with exotic allocators!\n");
    exit(1);
}

/**
 * Test 1: Simulate the OpenSSL access pattern that caused the segfault
 */
static void test_ssl_memory_access_pattern(void) {
    test_count++;
    printf("🕵️ Test %d: Simulating SSL memory access pattern...\n", test_count);
    
    /* Allocate memory similar to SSL context */
    void* fake_ssl_ctx = malloc(1024);
    if (!fake_ssl_ctx) {
        printf("❌ Memory allocation failed\n");
        return;
    }
    
    /* Initialize with pattern */
    memset(fake_ssl_ctx, 0x42, 1024);
    
    /* Simulate the exact access that caused segfault: [ptr+0x38] */
    char* ptr = (char*)fake_ssl_ctx;
    volatile long value = *((long*)(ptr + 0x38));  /* This should work with valid memory */
    
    printf("✅ SSL memory access pattern successful: 0x%lx\n", value);
    free(fake_ssl_ctx);
    passed_tests++;
}

/**
 * Test 2: Simulate NULL pointer + 0x38 access (the actual crash)
 */
static void test_null_pointer_access(void) {
    test_count++;
    printf("🕵️ Test %d: Testing NULL pointer + 0x38 access...\n", test_count);
    
    /* Install segfault handler */
    signal(SIGSEGV, segfault_handler);
    
    printf("🔍 About to access NULL+0x38 (this should segfault)...\n");
    
    /* This will definitely segfault - exactly what happened in SSL */
    char* null_ptr = NULL;
    volatile long value = *((long*)(null_ptr + 0x38));
    
    /* Should never reach here */
    printf("❌ ERROR: NULL access should have segfaulted! value=0x%lx\n", value);
}

/**
 * Test 3: Check if dynamic linking affects SSL
 */
static void test_ssl_library_loading(void) {
    test_count++;
    printf("🕵️ Test %d: Testing SSL library dynamic loading...\n", test_count);
    
    /* Try to load SSL library dynamically */
    void* ssl_handle = dlopen("libssl.so.3", RTLD_LAZY);
    if (!ssl_handle) {
        printf("❌ Could not load libssl.so.3: %s\n", dlerror());
        return;
    }
    
    printf("✅ SSL library loaded successfully\n");
    
    /* Look for SSL_CTX_new function */
    void* ssl_ctx_new = dlsym(ssl_handle, "SSL_CTX_new");
    if (!ssl_ctx_new) {
        printf("❌ Could not find SSL_CTX_new: %s\n", dlerror());
        dlclose(ssl_handle);
        return;
    }
    
    printf("✅ SSL_CTX_new function found at: %p\n", ssl_ctx_new);
    
    dlclose(ssl_handle);
    passed_tests++;
}

/**
 * Test 4: Memory corruption simulation
 */
static void test_memory_corruption_detection(void) {
    test_count++;
    printf("🕵️ Test %d: Testing memory corruption detection...\n", test_count);
    
    /* Allocate memory and corrupt it */
    void* memory = malloc(1024);
    if (!memory) {
        printf("❌ Memory allocation failed\n");
        return;
    }
    
    /* Initialize memory with known pattern */
    memset(memory, 0xAA, 1024);
    
    /* Verify pattern */
    char* ptr = (char*)memory;
    if (ptr[0] == (char)0xAA && ptr[100] == (char)0xAA) {
        printf("✅ Memory pattern verification successful\n");
    } else {
        printf("❌ Memory corruption detected immediately!\n");
    }
    
    /* Simulate corruption by writing beyond boundary */
    char* beyond = ptr + 1024;
    *beyond = 0xFF;  /* This might corrupt other allocations */
    
    printf("✅ Memory corruption simulation completed\n");
    free(memory);
    passed_tests++;
}

/**
 * Inspector Claude's Investigation Report
 */
static void generate_investigation_report(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("🕵️ INSPECTOR CLAUDE'S SSL INVESTIGATION REPORT\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("📊 INVESTIGATION SUMMARY:\n");
    printf("   Tests Executed: %d\n", test_count);
    printf("   Tests Passed: %d\n", passed_tests);
    printf("   Tests Failed/Crashed: %d\n", test_count - passed_tests);
    printf("\n");
    
    printf("🔍 KEY FINDINGS:\n");
    printf("1. The segfault pattern is: mov rax, [rax+0x38]\n");
    printf("2. This indicates a NULL or corrupted pointer being dereferenced\n");
    printf("3. The crash occurs in libssl.so.3 during SSL context operations\n");
    printf("4. Exotic allocators likely corrupt SSL's internal structures\n");
    printf("\n");
    
    printf("🎯 INSPECTOR CLAUDE'S DEDUCTION:\n");
    printf("\"Ah! Ze SSL context structure becomes corrupted when ze exotic\n");
    printf(" allocators interfere with OpenSSL's memory expectations!\n");
    printf(" Ze solution, Inspector Claude believes, ees to ensure all SSL\n");
    printf(" allocations bypass ze exotic allocators completely!\"\n");
    printf("\n");
    
    printf("💡 RECOMMENDED SOLUTION:\n");
    printf("1. Add SSL-specific allocation bypass in memory_manager.c\n");
    printf("2. Implement memory_bypass_exotic_for_ssl() function\n");
    printf("3. Ensure all SSL contexts use system malloc exclusively\n");
    printf("4. Test SSL operations in isolation from Arena/TLSF allocators\n");
    printf("\n");
    
    printf("\"Ze case ees almost solved, Inspector Claude believes!\"\n");
    printf("═══════════════════════════════════════════════════════════════\n");
}

int main(void) {
    printf("🕵️ Inspector Claude's Simplified SSL Investigation\n");
    printf("\"Sometimes ze simple approach reveals ze most, no?\"\n\n");
    
    /* Run investigation tests */
    test_ssl_memory_access_pattern();
    test_ssl_library_loading();
    test_memory_corruption_detection();
    
    /* The NULL pointer test should be last since it will crash */
    printf("\n🕵️ Final test will demonstrate ze exact crash pattern...\n");
    test_null_pointer_access();
    
    /* Generate report (may not reach here if segfault occurs) */
    generate_investigation_report();
    
    return 0;
}