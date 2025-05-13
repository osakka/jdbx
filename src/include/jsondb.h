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
#include "components/database/database.h"
#include "components/api/api.h"

/* Optional JavaScript integration */
#ifdef USE_QUICKJS
#include "components/js/js_api.h"
#include "components/js/js_engine.h"
#endif

/* Transaction management */
#include "components/transaction/transaction.h"
#include "components/transaction/transaction_retry.h"

/* Query language */
#include "components/query/query_language.h"

/* Authentication and authorization */
#include "components/rbac/rbac.h"
#include "components/rbac/jwt.h"

/* Common utilities */
#include "components/utils/json.h"
#include "components/utils/logger.h"
#include "components/utils/config_loader.h"

#ifdef __cplusplus
}
#endif

#endif /* JSONDB_H */