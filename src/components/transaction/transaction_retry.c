#include "transaction/transaction.h"
#include "transaction/transaction_retry.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils/buffer_pool.h"

/* Transaction retry state structure */
struct transaction_retry_state {
  int attempt;        /* Current attempt number (0 = first attempt) */
  int next_delay_ms;     /* Delay for the next retry in milliseconds */
  transaction_retry_policy_t policy; /* Retry policy to use */
  int deadlock_detected;   /* Whether a deadlock was detected */
  time_t first_attempt_time; /* Time of the first attempt */
  int operations_succeeded;  /* Number of operations that succeeded before failure */
};

/* Default retry policy */
static transaction_retry_policy_t default_retry_policy = {
  .max_attempts = 3,      /* Retry up to 3 times */
  .base_delay_ms = 100,    /* Start with 100ms delay */
  .max_delay_ms = 2000,    /* Maximum 2 second delay */
  .jitter_factor_percent = 20, /* 20% random jitter */
  .deadlock_prioritized = 1  /* Prioritize deadlock retries */
};

/* Forward declaration moved to header file */

/* Calculate the next retry delay using exponential backoff with jitter */
static int calculate_retry_delay(transaction_retry_state_t* retry_state) {
  if (!retry_state) {
    return default_retry_policy.base_delay_ms;
  }
  
  /* Base delay with exponential backoff */
  int delay = retry_state->policy.base_delay_ms * (1 << retry_state->attempt);
  
  /* Cap at maximum delay */
  if (delay > retry_state->policy.max_delay_ms) {
    delay = retry_state->policy.max_delay_ms;
  }
  
  /* Add jitter to avoid thundering herd problems */
  if (retry_state->policy.jitter_factor_percent > 0) {
    int jitter_range = (delay * retry_state->policy.jitter_factor_percent) / 100;
    int jitter = rand() % (jitter_range * 2 + 1) - jitter_range;
    delay += jitter;
    
    /* Ensure delay doesn't go below base_delay_ms or above max_delay_ms */
    if (delay < retry_state->policy.base_delay_ms) {
      delay = retry_state->policy.base_delay_ms;
    } else if (delay > retry_state->policy.max_delay_ms) {
      delay = retry_state->policy.max_delay_ms;
    }
  }
  
  /* If this was a deadlock and we prioritize deadlock retries, reduce the delay */
  if (retry_state->deadlock_detected && retry_state->policy.deadlock_prioritized) {
    delay = delay / 2;
  }
  
  return delay;
}

/* Initialize a transaction retry state */
transaction_retry_state_t* transaction_retry_init(transaction_retry_policy_t* policy) {
  transaction_retry_state_t* retry_state = (transaction_retry_state_t*)BUFFER_ALLOC(sizeof(transaction_retry_state_t));
  if (!retry_state) {
    return NULL;
  }
  
  /* Initialize retry state */
  retry_state->attempt = 0;
  retry_state->next_delay_ms = 0;
  retry_state->deadlock_detected = 0;
  retry_state->first_attempt_time = time(NULL);
  retry_state->operations_succeeded = 0;
  
  /* Use provided policy or default */
  if (policy) {
    retry_state->policy = *policy;
  } else {
    retry_state->policy = default_retry_policy;
  }
  
  /* Calculate initial delay */
  retry_state->next_delay_ms = calculate_retry_delay(retry_state);
  
  return retry_state;
}

/* Free a transaction retry state */
void transaction_retry_free(transaction_retry_state_t* retry_state) {
  if (retry_state) {
    BUFFER_FREE(retry_state);
  }
}

/* Check if a transaction should be retried */
int transaction_should_retry(transaction_retry_state_t* retry_state, int error_code) {
  if (!retry_state) {
    return 0;
  }
  
  /* If we've reached the maximum attempts, don't retry */
  if (retry_state->attempt >= retry_state->policy.max_attempts) {
    return 0;
  }
  
  /* Determine if the error is retryable */
  int retryable_error = 0;
  retry_state->deadlock_detected = 0;
  
  switch (error_code) {
    case -1: /* Deadlock error */
      retryable_error = 1;
      retry_state->deadlock_detected = 1;
      break;
      
    case -2: /* Lock timeout error */
      retryable_error = 1;
      break;
      
    case -3: /* Transaction conflict error */
      retryable_error = 1;
      break;
      
    case -4: /* Temporary server error */
      retryable_error = 1;
      break;
      
    default:
      /* Other errors are not retryable */
      retryable_error = 0;
      break;
  }
  
  return retryable_error;
}

/* Prepare for the next retry attempt */
void transaction_retry_next(transaction_retry_state_t* retry_state) {
  if (!retry_state) {
    return;
  }
  
  /* Increment attempt counter */
  retry_state->attempt++;
  
  /* Calculate next retry delay */
  retry_state->next_delay_ms = calculate_retry_delay(retry_state);
  
  /* Reset operations succeeded counter */
  retry_state->operations_succeeded = 0;
}

/* Wait for the next retry attempt using the calculated delay */
void transaction_retry_wait(transaction_retry_state_t* retry_state) {
  if (!retry_state || retry_state->next_delay_ms <= 0) {
    return;
  }
  
  /* Sleep for the calculated delay */
  usleep(retry_state->next_delay_ms * 1000); /* Convert ms to microseconds */
}

/* Record a successful operation for partial retry tracking */
void transaction_retry_record_success(transaction_retry_state_t* retry_state) {
  if (!retry_state) {
    return;
  }
  
  retry_state->operations_succeeded++;
}

/* Get the number of successful operations to skip on retry */
int transaction_retry_get_skip_count(transaction_retry_state_t* retry_state) {
  if (!retry_state) {
    return 0;
  }
  
  return retry_state->operations_succeeded;
}

/* Get current retry statistics */
void transaction_retry_get_stats(transaction_retry_state_t* retry_state, 
                int* attempts, int* delay_ms, int* max_attempts) {
  if (!retry_state) {
    return;
  }
  
  if (attempts) {
    *attempts = retry_state->attempt;
  }
  
  if (delay_ms) {
    *delay_ms = retry_state->next_delay_ms;
  }
  
  if (max_attempts) {
    *max_attempts = retry_state->policy.max_attempts;
  }
}

/* Example of how to use the retry system in a transaction */
void transaction_retry_example(transaction_manager_t* manager) {
  /* Initialize retry state with default policy */
  transaction_retry_state_t* retry_state = transaction_retry_init(NULL);
  if (!retry_state) {
    printf("Failed to initialize retry state\n");
    return;
  }
  
  transaction_t* transaction = NULL;
  int error_code = 0;
  int success = 0;
  
  /* Try up to max_attempts times */
  do {
    /* Begin a new transaction */
    transaction = transaction_begin(manager, ISOLATION_SERIALIZABLE, "user123");
    if (!transaction) {
      error_code = -4; /* Temporary server error */
      goto handle_failure;
    }
    
    /* Skip operations that already succeeded in previous attempts */
    int skip_count = transaction_retry_get_skip_count(retry_state);
    
    /* Perform transaction operations */
    if (skip_count == 0) {
      /* First operation */
      if (!transaction_insert_document(manager, transaction, "collection1", NULL)) {
        error_code = -2; /* Lock timeout error */
        goto handle_failure;
      }
      transaction_retry_record_success(retry_state);
    }
    
    if (skip_count <= 1) {
      /* Second operation */
      if (!transaction_update_document(manager, transaction, "collection2", "doc1", NULL)) {
        error_code = -1; /* Deadlock error */
        goto handle_failure;
      }
      transaction_retry_record_success(retry_state);
    }
    
    if (skip_count <= 2) {
      /* Third operation */
      if (!transaction_delete_document(manager, transaction, "collection3", "doc2")) {
        error_code = -3; /* Transaction conflict error */
        goto handle_failure;
      }
      transaction_retry_record_success(retry_state);
    }
    
    /* Commit the transaction */
    if (!transaction_commit(manager, transaction)) {
      error_code = -3; /* Transaction conflict error */
      goto handle_failure;
    }
    
    /* Transaction succeeded */
    success = 1;
    break;
    
  handle_failure:
    /* Rollback the transaction if it exists */
    if (transaction) {
      transaction_rollback(manager, transaction);
    }
    
    /* Check if we should retry */
    if (transaction_should_retry(retry_state, error_code)) {
      /* Prepare for the next retry attempt */
      transaction_retry_next(retry_state);
      
      /* Get retry statistics */
      int attempts, delay_ms, max_attempts;
      transaction_retry_get_stats(retry_state, &attempts, &delay_ms, &max_attempts);
      
      printf("Transaction failed with error %d, retrying attempt %d/%d after %d ms\n",
         error_code, attempts, max_attempts, delay_ms);
      
      /* Wait before retrying */
      transaction_retry_wait(retry_state);
    } else {
      /* Transaction failed and should not be retried */
      printf("Transaction failed with error %d, not retrying\n", error_code);
      break;
    }
  } while (!success);
  
  /* Cleanup */
  transaction_retry_free(retry_state);
  
  if (success) {
    printf("Transaction completed after %d attempts\n", retry_state->attempt + 1);
  } else {
    printf("Transaction failed after %d attempts\n", retry_state->attempt + 1);
  }
}