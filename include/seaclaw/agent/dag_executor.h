#ifndef SC_DAG_EXECUTOR_H
#define SC_DAG_EXECUTOR_H

#include "seaclaw/agent/dag.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/tool.h"
#include <stddef.h>

#define SC_DAG_MAX_BATCH_SIZE 8

typedef struct sc_dag_batch {
    sc_dag_node_t *nodes[SC_DAG_MAX_BATCH_SIZE];
    size_t count;
} sc_dag_batch_t;

sc_error_t sc_dag_next_batch(sc_dag_t *dag, sc_dag_batch_t *batch);
sc_error_t sc_dag_resolve_vars(sc_allocator_t *alloc, const sc_dag_t *dag, const char *args,
                               size_t args_len, char **resolved, size_t *resolved_len);

#endif
