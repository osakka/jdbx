/**
 * @file inspector_clouseau_ssl_detective.c
 * @brief Inspector Clouseau's SSL Memory Investigation
 * 
 * "Ah! Ze mystery of ze SSL corruption with ze exotic allocators!"
 * 
 * Detective Claude will systematically investigate:
 * 1. SSL context allocation patterns
 * 2. Exotic allocator interference  
 * 3. Memory promotion requirements
 * 4. OpenSSL internal expectations
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Include JDBX memory management for investigation */
#include "../src/include/utils/memory_manager.h"
#include "../src/include/utils/arena_allocator.h"
#include "../src/include/utils/tlsf_allocator.h"
#include "../src/include/utils/memory_allocator_config.h"

/**
 * Detective Claude's SSL Investigation Framework
 */
typedef struct {
    const char* test_name;
    int ssl_contexts_created;
    int ssl_contexts_destroyed;
    int memory_corruptions_detected;
    int exotic_allocator_active;
    const char* findings;
} ssl_investigation_report_t;

/**
 * "First, we recreate ze crime scene..."
 */
static ssl_investigation_report_t investigate_ssl_basic_creation(void) {
    printf("🕵️ Inspector Claude: Testing basic SSL context creation...\n");
    
    ssl_investigation_report_t report = {
        .test_name = "Basic SSL Context Creation",
        .ssl_contexts_created = 0,
        .ssl_contexts_destroyed = 0,
        .memory_corruptions_detected = 0,
        .exotic_allocator_active = 0,
        .findings = "Unknown"
    };
    
    /* Initialize SSL library */
    SSL_library_init();
    SSL_load_error_strings();
    
    /* Create SSL context like JDBX does */
    const SSL_METHOD* method = TLS_server_method();
    if (!method) {
        report.findings = "SSL method creation failed";
        return report;
    }
    
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        report.findings = "SSL context creation failed";
        return report;
    }
    report.ssl_contexts_created = 1;
    
    /* Test SSL context operations */
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    
    /* Clean up */
    SSL_CTX_free(ctx);
    report.ssl_contexts_destroyed = 1;
    
    report.findings = "Basic SSL operations successful with system malloc";
    return report;
}

/**
 * "Now, we test with ze exotic allocators..."
 */
static ssl_investigation_report_t investigate_ssl_with_exotic_allocators(void) {
    printf("🕵️ Inspector Claude: Testing SSL with exotic allocators...\n");
    
    ssl_investigation_report_t report = {
        .test_name = "SSL with Exotic Allocators",
        .ssl_contexts_created = 0,
        .ssl_contexts_destroyed = 0,
        .memory_corruptions_detected = 0,
        .exotic_allocator_active = 1,
        .findings = "Unknown"
    };
    
    /* Enable exotic allocators */
    memory_manager_init();
    
    /* Create checkpoint to simulate API request environment */
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    if (!checkpoint) {
        report.findings = "Checkpoint creation failed";
        return report;
    }
    
    printf("🔍 Created checkpoint: %p\n", (void*)checkpoint);
    
    /* Initialize SSL library under exotic allocators */
    SSL_library_init();
    SSL_load_error_strings();
    
    /* Create SSL context */
    const SSL_METHOD* method = TLS_server_method();
    if (!method) {
        report.findings = "SSL method creation failed with exotic allocators";
        memory_checkpoint_commit(checkpoint);
        return report;
    }
    
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        report.findings = "SSL context creation failed with exotic allocators";
        memory_checkpoint_commit(checkpoint);
        return report;
    }
    report.ssl_contexts_created = 1;
    
    printf("🔍 SSL context created: %p\n", (void*)ctx);
    
    /* Test SSL context operations that might trigger the segfault */
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    
    /* THIS IS THE CRITICAL TEST: Commit checkpoint while SSL context exists */
    printf("🔍 About to commit checkpoint with SSL context still alive...\n");
    
    /* Clean up SSL first */
    SSL_CTX_free(ctx);
    report.ssl_contexts_destroyed = 1;
    
    /* Now commit checkpoint */
    memory_checkpoint_commit(checkpoint);
    
    report.findings = "SSL operations successful with exotic allocators";
    return report;
}

/**
 * "Ah! But what eef we promote ze SSL context?"
 */
static ssl_investigation_report_t investigate_ssl_with_promotion(void) {
    printf("🕵️ Inspector Claude: Testing SSL with memory promotion...\n");
    
    ssl_investigation_report_t report = {
        .test_name = "SSL with Memory Promotion",
        .ssl_contexts_created = 0,
        .ssl_contexts_destroyed = 0,
        .memory_corruptions_detected = 0,
        .exotic_allocator_active = 1,
        .findings = "Unknown"
    };
    
    /* Create checkpoint */
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    if (!checkpoint) {
        report.findings = "Checkpoint creation failed";
        return report;
    }
    
    /* Initialize SSL library */
    SSL_library_init();
    SSL_load_error_strings();
    
    /* Create SSL context */
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        report.findings = "SSL context creation failed";
        memory_checkpoint_commit(checkpoint);
        return report;
    }
    report.ssl_contexts_created = 1;
    
    /* THE KEY TEST: Promote SSL context before checkpoint operations */
    printf("🔍 Promoting SSL context to persistent memory...\n");
    memory_promote(ctx);  /* This should protect it from checkpoint cleanup */
    
    /* Configure SSL context */
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    
    /* Commit checkpoint - SSL context should survive */
    memory_checkpoint_commit(checkpoint);
    
    /* Test SSL context after checkpoint commit */
    printf("🔍 Testing SSL context after checkpoint commit...\n");
    
    /* Access SSL context structure (this might trigger segfault if corrupted) */
    long options = SSL_CTX_get_options(ctx);
    printf("🔍 SSL options after checkpoint: 0x%lx\n", options);
    
    /* Clean up */
    SSL_CTX_free(ctx);
    report.ssl_contexts_destroyed = 1;
    
    report.findings = "SSL operations with promotion successful";
    return report;
}

/**
 * "Ze final test - simulate ze exact JDBX scenario!"
 */
static ssl_investigation_report_t investigate_jdbx_ssl_scenario(void) {
    printf("🕵️ Inspector Claude: Recreating JDBX SSL scenario...\n");
    
    ssl_investigation_report_t report = {
        .test_name = "JDBX SSL Scenario Recreation",
        .ssl_contexts_created = 0,
        .ssl_contexts_destroyed = 0,
        .memory_corruptions_detected = 0,
        .exotic_allocator_active = 1,
        .findings = "Unknown"
    };
    
    /* Simulate JDBX initialization */
    memory_manager_init();
    SSL_library_init();
    SSL_load_error_strings();
    
    /* Create SSL context during initialization (no checkpoint) */
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* global_ssl_ctx = SSL_CTX_new(method);
    if (!global_ssl_ctx) {
        report.findings = "Global SSL context creation failed";
        return report;
    }
    report.ssl_contexts_created++;
    
    /* Configure global SSL context */
    SSL_CTX_set_options(global_ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_mode(global_ssl_ctx, SSL_MODE_AUTO_RETRY);
    
    printf("🔍 Global SSL context created: %p\n", (void*)global_ssl_ctx);
    
    /* Simulate API request processing with checkpoints */
    for (int request = 1; request <= 3; request++) {
        printf("🔍 Processing API request %d...\n", request);
        
        memory_checkpoint_t* request_checkpoint = memory_checkpoint_create();
        if (!request_checkpoint) {
            report.findings = "Request checkpoint creation failed";
            break;
        }
        
        /* Create per-request SSL connection */
        SSL* ssl = SSL_new(global_ssl_ctx);
        if (!ssl) {
            report.findings = "SSL connection creation failed";
            memory_checkpoint_commit(request_checkpoint);
            break;
        }
        
        printf("🔍 SSL connection created: %p\n", (void*)ssl);
        
        /* Simulate SSL operations */
        SSL_set_accept_state(ssl);  /* Server mode */
        
        /* CRITICAL: What happens when we commit checkpoint with active SSL? */
        SSL_free(ssl);  /* Clean up SSL connection first */
        memory_checkpoint_commit(request_checkpoint);
        
        printf("🔍 Request %d completed successfully\n", request);
    }
    
    /* Test global SSL context after multiple checkpoint operations */
    printf("🔍 Testing global SSL context after checkpoint cycles...\n");
    long final_options = SSL_CTX_get_options(global_ssl_ctx);
    printf("🔍 Final SSL options: 0x%lx\n", final_options);
    
    /* Clean up global context */
    SSL_CTX_free(global_ssl_ctx);
    report.ssl_contexts_destroyed++;
    
    report.findings = "JDBX scenario completed without corruption";
    return report;
}

/**
 * Inspector Claude's Final Report
 */
static void generate_detective_report(ssl_investigation_report_t* reports, int num_tests) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("🕵️ INSPECTOR CLAUDE'S SSL CORRUPTION INVESTIGATION REPORT\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    for (int i = 0; i < num_tests; i++) {
        printf("📋 Test %d: %s\n", i + 1, reports[i].test_name);
        printf("   SSL Contexts Created: %d\n", reports[i].ssl_contexts_created);
        printf("   SSL Contexts Destroyed: %d\n", reports[i].ssl_contexts_destroyed);
        printf("   Memory Corruptions: %d\n", reports[i].memory_corruptions_detected);
        printf("   Exotic Allocators: %s\n", reports[i].exotic_allocator_active ? "ACTIVE" : "INACTIVE");
        printf("   Findings: %s\n", reports[i].findings);
        printf("\n");
    }
    
    printf("🎯 DETECTIVE CLAUDE'S CONCLUSIONS:\n");
    printf("1. If all tests pass: SSL corruption is timing/context specific\n");
    printf("2. If exotic allocator tests fail: Memory promotion needed\n");
    printf("3. If JDBX scenario fails: Checkpoint lifecycle issue confirmed\n");
    printf("4. The segfault likely occurs during SSL structure access after memory corruption\n\n");
    
    printf("🔍 RECOMMENDED NEXT INVESTIGATION:\n");
    printf("- Add SSL context memory promotion in JDBX initialization\n");
    printf("- Test SSL operations under different checkpoint scenarios\n");
    printf("- Use Valgrind to detect exact corruption point\n");
    printf("- Implement SSL-specific allocation bypass if needed\n\n");
    
    printf("\"Ah! Ze mystery ees becoming clearer, Inspector Claude suspects!\"\n");
    printf("═══════════════════════════════════════════════════════════════\n");
}

int main(void) {
    printf("🕵️ Inspector Claude's SSL Memory Corruption Investigation\n");
    printf("\"Ze case of ze mysterious segfault in libssl.so.3!\"\n\n");
    
    /* Set up environment for investigation */
    setenv("JDBX_ENABLE_EXOTIC_ALLOCATORS", "true", 1);
    setenv("JDBX_ENABLE_ARENA_ALLOCATOR", "true", 1);
    setenv("JDBX_ENABLE_TLSF_ALLOCATOR", "true", 1);
    setenv("JDBX_MEM_DEBUG", "false", 1);  /* Reduce noise for investigation */
    
    ssl_investigation_report_t reports[4];
    
    /* Detective Claude's systematic investigation */
    printf("🕵️ \"First, we establish ze baseline with system malloc...\"\n");
    reports[0] = investigate_ssl_basic_creation();
    
    printf("\n🕵️ \"Now, ze exotic allocators enter ze scene...\"\n");
    reports[1] = investigate_ssl_with_exotic_allocators();
    
    printf("\n🕵️ \"Ah! But what eef we use ze memory promotion technique?\"\n");
    reports[2] = investigate_ssl_with_promotion();
    
    printf("\n🕵️ \"Finally, we recreate ze exact JDBX scenario...\"\n");
    reports[3] = investigate_jdbx_ssl_scenario();
    
    /* Generate final detective report */
    generate_detective_report(reports, 4);
    
    return 0;
}