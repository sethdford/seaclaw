#ifndef HU_MEMORY_SQL_TRANSACTION_H
#define HU_MEMORY_SQL_TRANSACTION_H

#include "human/core/error.h"
#include <stdbool.h>

struct sqlite3;

typedef struct hu_sql_txn {
    struct sqlite3 *db;
    bool active;
} hu_sql_txn_t;

hu_error_t hu_sql_txn_begin(hu_sql_txn_t *txn, struct sqlite3 *db);
hu_error_t hu_sql_txn_commit(hu_sql_txn_t *txn);
void       hu_sql_txn_rollback(hu_sql_txn_t *txn);

#endif
