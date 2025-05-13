#ifndef TRANSACTION_RETRY_H
#define TRANSACTION_RETRY_H

#include "transaction/transaction.h"

/* Forward declaration */
typedef struct transaction_retry_state transaction_retry_state_t;

/* Transaction retry policy */
typedef struct {
    int max_attempts;           /* Maximum number of retry attempts */
    int base_delay_ms;          /* Base delay between retries in milliseconds */
    int max_delay_ms;           /* Maximum delay between retries in milliseconds */
    int jitter_factor_percent;  /* Random jitter factor as percentage of delay */
    int deadlock_prioritized;   /* Whether deadlock aborts get priority retry handling */
} transaction_retry_policy_t;

/* Initialize a transaction retry state with default policy */
transaction_retry_state_t* transaction_retry_init(transaction_retry_policy_t* policy);

/* Free a transaction retry state */
void transaction_retry_free(transaction_retry_state_t* retry_state);

/* Check if a transaction should be retried based on the error code */
int transaction_should_retry(transaction_retry_state_t* retry_state, int error_code);

/* Prepare for the next retry attempt */
void transaction_retry_next(transaction_retry_state_t* retry_state);

/* Wait for the next retry attempt using the calculated delay */
void transaction_retry_wait(transaction_retry_state_t* retry_state);

/* Record a successful operation for partial retry tracking */
void transaction_retry_record_success(transaction_retry_state_t* retry_state);

/* Get the number of successful operations to skip on retry */
int transaction_retry_get_skip_count(transaction_retry_state_t* retry_state);

/* Get current retry statistics */
void transaction_retry_get_stats(transaction_retry_state_t* retry_state, 
                               int* attempts, int* delay_ms, int* max_attempts);

/* Example of how to use the retry system in a transaction */
void transaction_retry_example(transaction_manager_t* manager);

/* Error codes for retryable errors */
#define RETRY_ERROR_DEADLOCK          -1
#define RETRY_ERROR_LOCK_TIMEOUT      -2
#define RETRY_ERROR_CONFLICT          -3
#define RETRY_ERROR_TEMPORARY_SERVER  -4

#endif /* TRANSACTION_RETRY_H */