#include "human/memory/sql_transaction.h"
#include <sqlite3.h>
#include <stddef.h>

hu_error_t hu_sql_txn_begin(hu_sql_txn_t *txn, struct sqlite3 *db) {
    if (!txn || !db)
        return HU_ERR_INVALID_ARGUMENT;
    txn->db = db;
    txn->active = false;
    int rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    txn->active = true;
    return HU_OK;
}

hu_error_t hu_sql_txn_commit(hu_sql_txn_t *txn) {
    if (!txn || !txn->active)
        return HU_ERR_INVALID_ARGUMENT;
    int rc = sqlite3_exec(txn->db, "COMMIT", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    txn->active = false;
    return HU_OK;
}

void hu_sql_txn_rollback(hu_sql_txn_t *txn) {
    if (!txn || !txn->active)
        return;
    sqlite3_exec(txn->db, "ROLLBACK", NULL, NULL, NULL);
    txn->active = false;
}
