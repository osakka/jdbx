/**
 * @file jdbx.h
 * @brief Main header file for the JDBX library
 *
 * This header includes all the necessary components for using JDBX.
 * For more specific functionality, include the appropriate component headers.
 */

#ifndef JDBX_H
#define JDBX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Core database functionality */
#include "database/database.h"
#include "api/api.h"

/* Optional JavaScript integration */
#ifdef USE_QUICKJS
#include "js/js_api.h"
#include "js/js_engine.h"
#endif

/* Transaction management */
#include "transaction/transaction.h"
#include "transaction/transaction_retry.h"

/* Query language */
#include "query/query_language.h"

/* Authentication and authorization */
#include "rbac/rbac.h"
#include "rbac/jwt.h"

/* Common utilities */
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/config_loader.h"

#ifdef __cplusplus
}
#endif

#endif /* JDBX_H */