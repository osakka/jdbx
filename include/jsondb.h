/**
 * @file jsondb.h
 * @brief Main header file for the JSONdb library
 *
 * This header includes all the necessary components for using JSONdb.
 * For more specific functionality, include the appropriate component headers.
 */

#ifndef JSONDB_H
#define JSONDB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Core database functionality */
#include "jsondb/database/database.h"
#include "jsondb/api/api.h"

/* Optional JavaScript integration */
#ifdef USE_QUICKJS
#include "jsondb/js/js_api.h"
#include "jsondb/js/js_engine.h"
#endif

/* Transaction management */
#include "jsondb/transaction/transaction.h"
#include "jsondb/transaction/transaction_retry.h"

/* Query language */
#include "jsondb/query/query_language.h"

/* Authentication and authorization */
#include "jsondb/rbac/rbac.h"
#include "jsondb/rbac/jwt.h"

/* Common utilities */
#include "jsondb/utils/json.h"
#include "jsondb/utils/logger.h"
#include "jsondb/utils/config_loader.h"

#ifdef __cplusplus
}
#endif

#endif /* JSONDB_H */