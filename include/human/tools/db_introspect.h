#ifndef HU_TOOLS_DB_INTROSPECT_H
#define HU_TOOLS_DB_INTROSPECT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"

/**
 * Database schema introspection tool.
 *
 * Supports:
 * - tables: List all tables in the database
 * - columns: List columns for a specific table with types, constraints
 * - indexes: List indexes for a specific table
 * - foreign_keys: List foreign key relationships for a specific table
 *
 * Parameters:
 * - action: "tables" | "columns" | "indexes" | "foreign_keys"
 * - table: optional table name (required for columns/indexes/foreign_keys)
 * - database: optional database path (default: in-memory)
 */

hu_error_t hu_db_introspect_tool_create(hu_allocator_t *alloc, const char *default_db_path,
                                        hu_tool_t *out);

#endif /* HU_TOOLS_DB_INTROSPECT_H */
