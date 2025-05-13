#include "transaction/transaction.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* Deadlock test scenario structure */
typedef struct {
    const char* name;               /* Scenario name */
    int transaction_count;          /* Number of transactions */
    int operation_count;            /* Number of operations */
    int (*setup)(void*);            /* Setup function */
    int (*verify)(void*);           /* Verification function */
    void* data;                     /* Test-specific data */
    int expected_deadlocks;         /* Expected number of deadlocks */
    int timeout_seconds;            /* Test timeout in seconds */
} deadlock_test_t;

/* Thread argument for running transactions */
typedef struct {
    transaction_manager_t* manager;   /* Transaction manager */
    transaction_t* transaction;       /* Transaction */
    int transaction_id;               /* Thread/transaction ID */
    const char** collections;         /* Collections to use */
    const char** document_ids;        /* Document IDs to use */
    int operation_count;              /* Number of operations to perform */
    int* operation_sequence;          /* Sequence of operations to execute */
    isolation_level_t isolation_level;/* Isolation level */
    int wait_between_ops_ms;          /* Wait time between operations */
    pthread_barrier_t* barrier;       /* Synchronization barrier */
    int result;                       /* Result code */
} transaction_thread_arg_t;

/* Operation types for test scenarios */
typedef enum {
    TEST_OP_LOCK_DOC,         /* Lock a document (exclusive) */
    TEST_OP_LOCK_DOC_SHARED,  /* Lock a document (shared) */
    TEST_OP_UNLOCK_DOC,       /* Unlock a document */
    TEST_OP_WAIT,             /* Wait for some time */
    TEST_OP_COMMIT,           /* Commit the transaction */
    TEST_OP_ROLLBACK          /* Rollback the transaction */
} test_operation_t;

/* Define common collections and document IDs for tests */
static const char* test_collections[] = {
    "users", "products", "orders", "payments", "inventory"
};

static const char* test_document_ids[] = {
    "doc1", "doc2", "doc3", "doc4", "doc5", "doc6", "doc7", "doc8", "doc9", "doc10"
};

/* Worker thread for executing transaction operations */
void* transaction_worker(void* arg) {
    transaction_thread_arg_t* thread_arg = (transaction_thread_arg_t*)arg;
    if (!thread_arg) {
        return NULL;
    }

    /* Local variables */
    transaction_manager_t* manager = thread_arg->manager;
    int tx_id = thread_arg->transaction_id;
    const char** collections = thread_arg->collections;
    const char** document_ids = thread_arg->document_ids;
    int op_count = thread_arg->operation_count;
    int* op_seq = thread_arg->operation_sequence;
    int wait_time = thread_arg->wait_between_ops_ms;
    
    /* User ID for this transaction worker */
    char user_id[32];
    snprintf(user_id, sizeof(user_id), "test-user-%d", tx_id);
    
    /* Begin the transaction */
    thread_arg->transaction = transaction_begin(manager, thread_arg->isolation_level, user_id);
    if (!thread_arg->transaction) {
        printf("Transaction %d failed to begin\n", tx_id);
        thread_arg->result = -1;
        return NULL;
    }
    
    /* Wait for all threads to begin their transactions */
    pthread_barrier_wait(thread_arg->barrier);
    
    /* Execute the operation sequence */
    for (int i = 0; i < op_count; i++) {
        test_operation_t op = op_seq[i];
        int doc_index = i % 10;  /* Cycle through document IDs */
        int coll_index = (tx_id + i) % 5;  /* Cycle through collections, offset by tx_id */
        int result = 0;
        
        /* Execute the appropriate operation */
        switch (op) {
            case TEST_OP_LOCK_DOC:
                result = lock_manager_lock_document(
                    manager->lock_manager,
                    collections[coll_index],
                    document_ids[doc_index],
                    thread_arg->transaction
                );
                printf("Tx %d: Lock %s/%s: %s\n", 
                       tx_id, collections[coll_index], document_ids[doc_index],
                       result ? "SUCCESS" : "FAILED");
                break;
                
            case TEST_OP_LOCK_DOC_SHARED:
                result = lock_manager_lock_document_shared(
                    manager->lock_manager,
                    collections[coll_index],
                    document_ids[doc_index],
                    thread_arg->transaction
                );
                printf("Tx %d: Lock shared %s/%s: %s\n", 
                       tx_id, collections[coll_index], document_ids[doc_index],
                       result ? "SUCCESS" : "FAILED");
                break;
                
            case TEST_OP_UNLOCK_DOC:
                result = lock_manager_unlock_document(
                    manager->lock_manager,
                    collections[coll_index],
                    document_ids[doc_index],
                    thread_arg->transaction
                );
                printf("Tx %d: Unlock %s/%s: %s\n", 
                       tx_id, collections[coll_index], document_ids[doc_index],
                       result ? "SUCCESS" : "FAILED");
                break;
                
            case TEST_OP_WAIT:
                /* Just wait a bit to allow other transactions to run */
                usleep(wait_time * 1000);
                result = 1;
                break;
                
            case TEST_OP_COMMIT:
                result = transaction_commit(manager, thread_arg->transaction);
                printf("Tx %d: Commit: %s\n", tx_id, result ? "SUCCESS" : "FAILED");
                thread_arg->transaction = NULL;  /* Committed transaction is freed */
                break;
                
            case TEST_OP_ROLLBACK:
                result = transaction_rollback(manager, thread_arg->transaction);
                printf("Tx %d: Rollback: %s\n", tx_id, result ? "SUCCESS" : "FAILED");
                thread_arg->transaction = NULL;  /* Rolled back transaction is freed */
                break;
        }
        
        /* Check if the operation failed due to abort or deadlock */
        if (!result && thread_arg->transaction) {
            if (thread_arg->transaction->state == TRANSACTION_ABORTING) {
                printf("Tx %d: ABORTED due to deadlock\n", tx_id);
                transaction_rollback(manager, thread_arg->transaction);
                thread_arg->transaction = NULL;
                break;
            }
        }
        
        /* Sleep between operations */
        if (wait_time > 0) {
            usleep(wait_time * 1000);
        }
    }
    
    /* Clean up if transaction wasn't committed or rolled back */
    if (thread_arg->transaction) {
        transaction_rollback(manager, thread_arg->transaction);
        thread_arg->transaction = NULL;
    }
    
    thread_arg->result = 1;  /* Success */
    return NULL;
}

/* Run a deadlock test scenario */
int run_deadlock_test(deadlock_test_t* test) {
    if (!test || !test->name) {
        return 0;
    }
    
    printf("\n=== Running deadlock test: %s ===\n", test->name);
    printf("Expected deadlocks: %d\n", test->expected_deadlocks);
    
    /* Create a test database */
    database_t* db = database_create(":memory:");
    if (!db) {
        printf("Failed to create test database\n");
        return 0;
    }
    
    /* Create a transaction manager */
    transaction_manager_t* manager = transaction_manager_create(db, 20);
    if (!manager) {
        printf("Failed to create transaction manager\n");
        database_free(db);
        return 0;
    }
    
    /* Run setup function if provided */
    if (test->setup && !test->setup(test->data)) {
        printf("Test setup failed\n");
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    /* Reset deadlock statistics */
    lock_manager_reset_deadlock_stats(manager->lock_manager);
    
    /* Configure deadlock detection */
    lock_manager_configure(manager->lock_manager, 1, 5000);  /* Enable, 5 second timeout */
    
    /* Create worker threads for each transaction */
    pthread_t* threads = (pthread_t*)calloc(test->transaction_count, sizeof(pthread_t));
    transaction_thread_arg_t* thread_args = 
        (transaction_thread_arg_t*)calloc(test->transaction_count, sizeof(transaction_thread_arg_t));
    
    if (!threads || !thread_args) {
        printf("Failed to allocate memory for threads\n");
        free(threads);
        free(thread_args);
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    /* Create a barrier for synchronizing transaction starts */
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, test->transaction_count);
    
    /* The main part of this function will be implemented by each test scenario */
    /* Each scenario will define its own operation sequences */
    
    /* Get the test start time for timeout calculation */
    time_t start_time = time(NULL);
    time_t end_time = start_time + test->timeout_seconds;
    
    /* Check for deadlocks periodically */
    int deadlocks_detected = 0;
    
    while (time(NULL) < end_time) {
        /* Let the transactions continue for a while */
        sleep(1);
        
        /* Check for deadlocks */
        int result = lock_manager_check_deadlocks(manager->lock_manager);
        if (result) {
            deadlocks_detected += result;
        }
        
        /* If we've detected enough deadlocks, stop */
        if (deadlocks_detected >= test->expected_deadlocks) {
            break;
        }
    }
    
    /* Analyze results */
    json_value_t* stats = lock_manager_get_stats(manager->lock_manager);
    if (stats) {
        json_value_t* dl_stats = json_object_get(stats, "deadlock_stats");
        if (dl_stats) {
            json_value_t* total = json_object_get(dl_stats, "total_deadlocks");
            if (total && total->type == JSON_INTEGER) {
                deadlocks_detected = total->integer_value;
            }
        }
        
        /* Print statistics */
        char* stats_str = json_stringify(stats);
        if (stats_str) {
            printf("\nDeadlock statistics: %s\n", stats_str);
            free(stats_str);
        }
        
        json_free(stats);
    }
    
    /* Cleanup */
    for (int i = 0; i < test->transaction_count; i++) {
        if (thread_args[i].transaction) {
            transaction_rollback(manager, thread_args[i].transaction);
        }
    }
    
    pthread_barrier_destroy(&barrier);
    free(threads);
    free(thread_args);
    transaction_manager_free(manager);
    database_free(db);
    
    /* Run verification if provided */
    if (test->verify && !test->verify(test->data)) {
        printf("Test verification failed\n");
        return 0;
    }
    
    /* Check if we detected the expected number of deadlocks */
    if (deadlocks_detected != test->expected_deadlocks) {
        printf("Expected %d deadlocks, but detected %d\n", 
               test->expected_deadlocks, deadlocks_detected);
        return 0;
    }
    
    printf("Test PASSED: %s\n", test->name);
    return 1;
}

/* Simple deadlock scenario with two transactions */
int simple_deadlock_test() {
    printf("\n=== Running Simple Deadlock Test ===\n");
    
    /* Create a test database */
    database_t* db = database_create(":memory:");
    if (!db) {
        printf("Failed to create test database\n");
        return 0;
    }
    
    /* Create a transaction manager */
    transaction_manager_t* manager = transaction_manager_create(db, 20);
    if (!manager) {
        printf("Failed to create transaction manager\n");
        database_free(db);
        return 0;
    }
    
    /* Configure deadlock detection */
    lock_manager_configure(manager->lock_manager, 1, 5000);  /* Enable, 5 second timeout */
    
    /* Reset deadlock statistics */
    lock_manager_reset_deadlock_stats(manager->lock_manager);
    
    /* Start two transactions */
    transaction_t* tx1 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user1");
    transaction_t* tx2 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user2");
    
    if (!tx1 || !tx2) {
        printf("Failed to begin transactions\n");
        if (tx1) transaction_rollback(manager, tx1);
        if (tx2) transaction_rollback(manager, tx2);
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    /* Create the deadlock scenario:
     * 1. TX1 locks docA
     * 2. TX2 locks docB
     * 3. TX1 tries to lock docB (waits for TX2)
     * 4. TX2 tries to lock docA (waits for TX1)
     * => Deadlock!
     */
    printf("TX1 locks docA\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docA", tx1)) {
        printf("TX1 failed to lock docA\n");
        transaction_rollback(manager, tx1);
        transaction_rollback(manager, tx2);
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    printf("TX2 locks docB\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docB", tx2)) {
        printf("TX2 failed to lock docB\n");
        transaction_rollback(manager, tx1);
        transaction_rollback(manager, tx2);
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    /* Create separate threads for the next locks to allow them to run concurrently */
    pthread_t thread1, thread2;
    
    /* Thread function for TX1 trying to lock docB */
    void* tx1_thread(void* arg) {
        printf("TX1 trying to lock docB (should wait for TX2)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docB", tx1);
        printf("TX1 lock docB result: %d\n", result);
        return NULL;
    }
    
    /* Thread function for TX2 trying to lock docA */
    void* tx2_thread(void* arg) {
        printf("TX2 trying to lock docA (should wait for TX1)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docA", tx2);
        printf("TX2 lock docA result: %d\n", result);
        return NULL;
    }
    
    /* Start the threads */
    pthread_create(&thread1, NULL, tx1_thread, NULL);
    
    /* Give TX1 a head start to ensure deterministic order */
    usleep(100000);  /* 100ms */
    
    pthread_create(&thread2, NULL, tx2_thread, NULL);
    
    /* Wait a bit for the deadlock to occur */
    usleep(500000);  /* 500ms */
    
    /* Check for deadlocks */
    printf("Checking for deadlocks...\n");
    int result = lock_manager_check_deadlocks(manager->lock_manager);
    printf("Deadlock check result: %d\n", result);
    
    /* Wait for threads to complete */
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    /* Get deadlock statistics */
    json_value_t* stats = lock_manager_get_stats(manager->lock_manager);
    if (stats) {
        char* stats_str = json_stringify(stats);
        if (stats_str) {
            printf("Deadlock statistics: %s\n", stats_str);
            free(stats_str);
        }
        json_free(stats);
    }
    
    /* Clean up */
    transaction_rollback(manager, tx1);
    transaction_rollback(manager, tx2);
    transaction_manager_free(manager);
    database_free(db);
    
    return result;  /* 1 if deadlock was detected and resolved */
}

/* Complex deadlock scenario with multiple transactions */
int complex_deadlock_test() {
    printf("\n=== Running Complex Deadlock Test ===\n");
    
    /* Create a test database */
    database_t* db = database_create(":memory:");
    if (!db) {
        printf("Failed to create test database\n");
        return 0;
    }
    
    /* Create a transaction manager */
    transaction_manager_t* manager = transaction_manager_create(db, 20);
    if (!manager) {
        printf("Failed to create transaction manager\n");
        database_free(db);
        return 0;
    }
    
    /* Configure deadlock detection */
    lock_manager_configure(manager->lock_manager, 1, 5000);  /* Enable, 5 second timeout */
    
    /* Reset deadlock statistics */
    lock_manager_reset_deadlock_stats(manager->lock_manager);
    
    /* Start four transactions */
    transaction_t* tx1 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user1");
    transaction_t* tx2 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user2");
    transaction_t* tx3 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user3");
    transaction_t* tx4 = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user4");
    
    if (!tx1 || !tx2 || !tx3 || !tx4) {
        printf("Failed to begin transactions\n");
        if (tx1) transaction_rollback(manager, tx1);
        if (tx2) transaction_rollback(manager, tx2);
        if (tx3) transaction_rollback(manager, tx3);
        if (tx4) transaction_rollback(manager, tx4);
        transaction_manager_free(manager);
        database_free(db);
        return 0;
    }
    
    /* Create a complex deadlock scenario with multiple cycles:
     * TX1 locks docA, waits for docB (held by TX2)
     * TX2 locks docB, waits for docC (held by TX3)
     * TX3 locks docC, waits for docD (held by TX4)
     * TX4 locks docD, waits for docA (held by TX1)
     * 
     * This forms a cycle: TX1 -> TX2 -> TX3 -> TX4 -> TX1
     */
    
    /* First, each TX acquires its primary lock */
    printf("TX1 locks docA\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docA", tx1)) {
        printf("TX1 failed to lock docA\n");
        goto cleanup;
    }
    
    printf("TX2 locks docB\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docB", tx2)) {
        printf("TX2 failed to lock docB\n");
        goto cleanup;
    }
    
    printf("TX3 locks docC\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docC", tx3)) {
        printf("TX3 failed to lock docC\n");
        goto cleanup;
    }
    
    printf("TX4 locks docD\n");
    if (!lock_manager_lock_document(manager->lock_manager, "collection1", "docD", tx4)) {
        printf("TX4 failed to lock docD\n");
        goto cleanup;
    }
    
    /* Create threads for the secondary locks */
    pthread_t thread1, thread2, thread3, thread4;
    
    /* Thread functions */
    void* tx1_thread(void* arg) {
        printf("TX1 trying to lock docB (should wait for TX2)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docB", tx1);
        printf("TX1 lock docB result: %d\n", result);
        return NULL;
    }
    
    void* tx2_thread(void* arg) {
        printf("TX2 trying to lock docC (should wait for TX3)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docC", tx2);
        printf("TX2 lock docC result: %d\n", result);
        return NULL;
    }
    
    void* tx3_thread(void* arg) {
        printf("TX3 trying to lock docD (should wait for TX4)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docD", tx3);
        printf("TX3 lock docD result: %d\n", result);
        return NULL;
    }
    
    void* tx4_thread(void* arg) {
        printf("TX4 trying to lock docA (should wait for TX1)\n");
        int result = lock_manager_lock_document(manager->lock_manager, "collection1", "docA", tx4);
        printf("TX4 lock docA result: %d\n", result);
        return NULL;
    }
    
    /* Start the threads in sequence with small delays */
    pthread_create(&thread1, NULL, tx1_thread, NULL);
    usleep(100000);  /* 100ms */
    
    pthread_create(&thread2, NULL, tx2_thread, NULL);
    usleep(100000);  /* 100ms */
    
    pthread_create(&thread3, NULL, tx3_thread, NULL);
    usleep(100000);  /* 100ms */
    
    pthread_create(&thread4, NULL, tx4_thread, NULL);
    
    /* Wait a bit for the deadlock to occur */
    usleep(500000);  /* 500ms */
    
    /* Check for deadlocks */
    printf("Checking for deadlocks...\n");
    int result = lock_manager_check_deadlocks(manager->lock_manager);
    printf("Deadlock check result: %d\n", result);
    
    /* Wait for threads to complete */
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    
    /* Get deadlock statistics */
    json_value_t* stats = lock_manager_get_stats(manager->lock_manager);
    if (stats) {
        char* stats_str = json_stringify(stats);
        if (stats_str) {
            printf("Deadlock statistics: %s\n", stats_str);
            free(stats_str);
        }
        json_free(stats);
    }
    
cleanup:
    /* Clean up */
    if (tx1) transaction_rollback(manager, tx1);
    if (tx2) transaction_rollback(manager, tx2);
    if (tx3) transaction_rollback(manager, tx3);
    if (tx4) transaction_rollback(manager, tx4);
    transaction_manager_free(manager);
    database_free(db);
    
    return result;  /* 1 if deadlock was detected and resolved */
}

/* Main test function */
int main(int argc, char** argv) {
    /* Initialize logger */
    logger_init(LOG_LEVEL_DEBUG, "deadlock_test.log");
    
    /* Run the tests */
    int simple_result = simple_deadlock_test();
    int complex_result = complex_deadlock_test();
    
    /* Print summary */
    printf("\n=== Deadlock Test Summary ===\n");
    printf("Simple deadlock test: %s\n", simple_result ? "PASSED" : "FAILED");
    printf("Complex deadlock test: %s\n", complex_result ? "PASSED" : "FAILED");
    
    /* Overall result */
    return (simple_result && complex_result) ? 0 : 1;
}