#ifndef BINARY_TRANSACTIONS_H
#define BINARY_TRANSACTIONS_H

#include "database/database.h"
#include <stdint.h>

/**
 * Binary transaction log functions
 * 
 * This header defines functions for binary transaction logging,
 * which provides significant performance improvements over text-based logging.
 */

/* Transaction operation types */
typedef enum {
    BINARY_TRANS_OP_INSERT   = 0,
    BINARY_TRANS_OP_UPDATE   = 1,
    BINARY_TRANS_OP_DELETE   = 2,
    BINARY_TRANS_OP_BEGIN    = 3,
    BINARY_TRANS_OP_COMMIT   = 4,
    BINARY_TRANS_OP_ROLLBACK = 5
} binary_transaction_op_type_t;

/**
 * Initialize binary transaction log
 *
 * @param path Path to the transaction log file
 * @return 1 on success, 0 on failure
 */
int binary_transaction_log_init(const char* path);

/**
 * Close binary transaction log
 */
void binary_transaction_log_close();

/**
 * Log a transaction operation
 *
 * @param transaction_id Transaction ID
 * @param op_type Operation type
 * @param collection Collection name
 * @param document_id Document ID
 * @param document Document data
 * @return 1 on success, 0 on failure
 */
int binary_transaction_log_operation(uint32_t transaction_id, binary_transaction_op_type_t op_type,
                                   const char* collection, const char* document_id,
                                   json_value_t* document);

/**
 * Log a simple transaction event (begin/commit/rollback)
 *
 * @param transaction_id Transaction ID
 * @param op_type Operation type
 * @return 1 on success, 0 on failure
 */
int binary_transaction_log_event(uint32_t transaction_id, binary_transaction_op_type_t op_type);

/**
 * Replay transaction log
 *
 * @param path Path to the transaction log file
 * @param db Database to apply transactions to
 * @return 1 on success, 0 on failure
 */
int binary_transaction_log_replay(const char* path, database_t* db);

/**
 * Initialize binary transaction operations
 */
void binary_transaction_init();

#endif /* BINARY_TRANSACTIONS_H */