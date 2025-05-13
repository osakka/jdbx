# Transaction Retry Mechanism

This document describes the transaction retry mechanism implemented in the JSON database.

## Overview

The transaction retry mechanism provides a framework for automatically retrying failed transactions with exponential backoff and jitter. This helps applications handle transient failures and deadlocks gracefully, improving overall system reliability.

## Key Features

- Configurable retry policies with maximum attempts and delays
- Exponential backoff algorithm with random jitter
- Prioritized retries for deadlock-related failures
- Partial transaction tracking to skip already completed operations
- Detailed retry statistics for monitoring and debugging

## Retry Policy Configuration

The retry policy controls how transactions are retried:

```c
typedef struct {
    int max_attempts;           /* Maximum number of retry attempts */
    int base_delay_ms;          /* Base delay between retries in milliseconds */
    int max_delay_ms;           /* Maximum delay between retries in milliseconds */
    int jitter_factor_percent;  /* Random jitter factor as percentage of delay */
    int deadlock_prioritized;   /* Whether deadlock aborts get priority retry handling */
} transaction_retry_policy_t;
```

Default values:
- `max_attempts`: 3 (retry up to 3 times)
- `base_delay_ms`: 100 (start with 100ms delay)
- `max_delay_ms`: 2000 (maximum 2 second delay)
- `jitter_factor_percent`: 20 (20% random jitter)
- `deadlock_prioritized`: 1 (prioritize deadlock retries)

## Retryable Error Codes

The following error codes are considered retryable:

- `RETRY_ERROR_DEADLOCK` (-1): Transaction aborted due to deadlock
- `RETRY_ERROR_LOCK_TIMEOUT` (-2): Lock acquisition timed out
- `RETRY_ERROR_CONFLICT` (-3): Transaction conflict with another transaction
- `RETRY_ERROR_TEMPORARY_SERVER` (-4): Temporary server error

## API Usage

### Basic Usage

```c
#include "transaction_retry.h"

void example_transaction(transaction_manager_t* manager) {
    // Initialize retry state with default policy
    transaction_retry_state_t* retry_state = transaction_retry_init(NULL);
    
    transaction_t* tx = NULL;
    int error_code = 0;
    int success = 0;
    
    do {
        // Begin a new transaction
        tx = transaction_begin(manager, ISOLATION_READ_COMMITTED, "user");
        if (!tx) {
            error_code = RETRY_ERROR_TEMPORARY_SERVER;
            goto handle_failure;
        }
        
        // Perform operations
        if (!transaction_insert_document(manager, tx, "collection", doc)) {
            error_code = RETRY_ERROR_LOCK_TIMEOUT;
            goto handle_failure;
        }
        
        // Commit transaction
        if (!transaction_commit(manager, tx)) {
            error_code = RETRY_ERROR_CONFLICT;
            goto handle_failure;
        }
        
        success = 1;
        break;
        
    handle_failure:
        // Rollback if needed
        if (tx) transaction_rollback(manager, tx);
        
        // Check if we should retry
        if (transaction_should_retry(retry_state, error_code)) {
            // Prepare for next attempt
            transaction_retry_next(retry_state);
            // Wait before retrying
            transaction_retry_wait(retry_state);
        } else {
            break;
        }
    } while (!success);
    
    // Cleanup
    transaction_retry_free(retry_state);
}
```

### Custom Retry Policy

```c
transaction_retry_policy_t custom_policy = {
    .max_attempts = 5,            // More retries
    .base_delay_ms = 50,          // Start with shorter delay
    .max_delay_ms = 5000,         // Allow longer maximum delay
    .jitter_factor_percent = 30,  // More jitter
    .deadlock_prioritized = 1     // Still prioritize deadlocks
};

transaction_retry_state_t* retry_state = transaction_retry_init(&custom_policy);
```

### Partial Transaction Tracking

The retry mechanism can track which operations have already succeeded, allowing you to skip them on retry attempts:

```c
// Skip operations that already succeeded in previous attempts
int skip_count = transaction_retry_get_skip_count(retry_state);

if (skip_count == 0) {
    // First operation
    if (!transaction_insert_document(manager, tx, "collection1", doc1)) {
        // Handle error
    }
    transaction_retry_record_success(retry_state);
}

if (skip_count <= 1) {
    // Second operation
    if (!transaction_update_document(manager, tx, "collection2", "id", doc2)) {
        // Handle error
    }
    transaction_retry_record_success(retry_state);
}
```

### Retry Statistics

You can retrieve statistics about the retry process:

```c
int attempts, delay_ms, max_attempts;
transaction_retry_get_stats(retry_state, &attempts, &delay_ms, &max_attempts);

printf("Attempt %d/%d with %dms delay\n", attempts, max_attempts, delay_ms);
```

## Exponential Backoff with Jitter

The retry mechanism uses exponential backoff with jitter to determine the delay between retry attempts:

1. **Base Calculation**: `delay = base_delay_ms * (2^attempt)`
2. **Max Cap**: `delay = min(delay, max_delay_ms)`
3. **Jitter**: `delay += random(-jitter_range, +jitter_range)`
   where `jitter_range = delay * jitter_factor_percent / 100`
4. **Deadlock Prioritization**: If `deadlock_prioritized` is enabled and a deadlock was detected, `delay = delay / 2`

This algorithm ensures that:
- Retries are spaced out exponentially to prevent system overload
- Random jitter helps prevent "thundering herd" problems when multiple clients retry simultaneously
- Deadlock-related failures are retried more quickly as they are often temporary conditions

## Best Practices

1. **Set Appropriate Timeouts**: Ensure your transaction timeout is longer than the maximum retry delay.

2. **Use Idempotent Operations**: Ensure operations can be safely repeated without side effects.

3. **Consider Application Context**: Adjust retry policies based on the criticality of the operation and user experience requirements.

4. **Monitor Retry Statistics**: Track retry frequencies to identify potential issues in your application design or database configuration.

5. **Handle Permanent Failures**: Have a fallback plan for when all retries are exhausted.

## Implementation Details

The retry mechanism is implemented in `transaction_retry.c` and `transaction_retry.h`. Key components include:

- `transaction_retry_state_t`: Tracks the current state of the retry process
- `transaction_retry_policy_t`: Configures the retry behavior
- `calculate_retry_delay()`: Implements the exponential backoff algorithm
- `transaction_should_retry()`: Determines if an error is retryable

## Performance Considerations

The retry mechanism adds minimal overhead to your transactions:

- Memory Usage: One small retry state structure per transaction
- CPU Usage: Simple calculations for retry delays
- Time Impact: Intentional delays between retries following the backoff algorithm

The positive impact on system stability typically far outweighs these minimal costs.