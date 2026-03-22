#include "human/agent/hula.h"
#include "human/agent/dag.h"
#include "human/agent/planner.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

/* ── Name tables ────────────────────────────────────────────────────────── */

static const char *const op_names[] = {
    [HU_HULA_CALL]     = "call",
    [HU_HULA_SEQ]      = "seq",
    [HU_HULA_PAR]      = "par",
    [HU_HULA_BRANCH]   = "branch",
    [HU_HULA_LOOP]     = "loop",
    [HU_HULA_DELEGATE]  = "delegate",
    [HU_HULA_EMIT]     = "emit",
};

static const char *const pred_names[] = {
    [HU_HULA_PRED_SUCCESS]      = "success",
    [HU_HULA_PRED_FAILURE]      = "failure",
    [HU_HULA_PRED_CONTAINS]     = "contains",
    [HU_HULA_PRED_NOT_CONTAINS] = "not_contains",
    [HU_HULA_PRED_ALWAYS]       = "always",
};

static const char *const status_names[] = {
    [HU_HULA_PENDING] = "pending",
    [HU_HULA_RUNNING] = "running",
    [HU_HULA_DONE]    = "done",
    [HU_HULA_FAILED]  = "failed",
    [HU_HULA_SKIPPED] = "skipped",
};

const char *hu_hula_op_name(hu_hula_op_t op) {
    if ((unsigned)op < HU_HULA_OP_COUNT) return op_names[op];
    return "unknown";
}

const char *hu_hula_pred_name(hu_hula_pred_t pred) {
    if ((unsigned)pred <= HU_HULA_PRED_ALWAYS) return pred_names[pred];
    return "unknown";
}

const char *hu_hula_status_name(hu_hula_status_t status) {
    if ((unsigned)status <= HU_HULA_SKIPPED) return status_names[status];
    return "unknown";
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

hu_error_t hu_hula_program_init(hu_hula_program_t *prog, hu_allocator_t alloc,
                                const char *name, size_t name_len) {
    if (!prog) return HU_ERR_INVALID_ARGUMENT;
    memset(prog, 0, sizeof(*prog));
    prog->alloc = alloc;
    prog->version = HU_HULA_VERSION;
    if (name && name_len > 0) {
        prog->name = hu_strndup(&alloc, name, name_len);
        if (!prog->name) return HU_ERR_OUT_OF_MEMORY;
        prog->name_len = name_len;
    }
    return HU_OK;
}

static void hula_node_clear(hu_allocator_t *alloc, hu_hula_node_t *n) {
    if (n->id)              hu_str_free(alloc, n->id);
    if (n->tool_name)       hu_str_free(alloc, n->tool_name);
    if (n->args_json)       hu_str_free(alloc, n->args_json);
    if (n->match_str)       hu_str_free(alloc, n->match_str);
    if (n->goal)            hu_str_free(alloc, n->goal);
    if (n->delegate_model)  hu_str_free(alloc, n->delegate_model);
    if (n->emit_key)        hu_str_free(alloc, n->emit_key);
    if (n->emit_value)      hu_str_free(alloc, n->emit_value);
    if (n->description)     hu_str_free(alloc, n->description);
    memset(n, 0, sizeof(*n));
}

void hu_hula_program_deinit(hu_hula_program_t *prog) {
    if (!prog) return;
    for (size_t i = 0; i < prog->node_count; i++)
        hula_node_clear(&prog->alloc, &prog->nodes[i]);
    if (prog->nodes)
        prog->alloc.free(prog->alloc.ctx, prog->nodes,
                         prog->node_cap * sizeof(hu_hula_node_t));
    if (prog->name) hu_str_free(&prog->alloc, prog->name);
    memset(prog, 0, sizeof(*prog));
}

hu_hula_node_t *hu_hula_program_alloc_node(hu_hula_program_t *prog, hu_hula_op_t op,
                                            const char *id) {
    if (!prog || prog->node_count >= HU_HULA_MAX_NODES) return NULL;
    if (prog->node_count >= prog->node_cap) {
        size_t new_cap = prog->node_cap ? prog->node_cap * 2 : 16;
        if (new_cap > HU_HULA_MAX_NODES) new_cap = HU_HULA_MAX_NODES;
        hu_hula_node_t *buf = prog->alloc.alloc(prog->alloc.ctx,
                                                  new_cap * sizeof(hu_hula_node_t));
        if (!buf) return NULL;
        memset(buf, 0, new_cap * sizeof(hu_hula_node_t));
        if (prog->nodes) {
            memcpy(buf, prog->nodes, prog->node_count * sizeof(hu_hula_node_t));
            prog->alloc.free(prog->alloc.ctx, prog->nodes,
                             prog->node_cap * sizeof(hu_hula_node_t));
        }
        /* Fix child pointers after realloc */
        ptrdiff_t delta = (char *)buf - (char *)prog->nodes;
        if (prog->nodes) {
            for (size_t i = 0; i < prog->node_count; i++) {
                for (size_t c = 0; c < buf[i].children_count; c++) {
                    if (buf[i].children[c])
                        buf[i].children[c] = (hu_hula_node_t *)((char *)buf[i].children[c] + delta);
                }
            }
        }
        if (prog->root)
            prog->root = (hu_hula_node_t *)((char *)prog->root + delta);
        prog->nodes = buf;
        prog->node_cap = new_cap;
    }
    hu_hula_node_t *n = &prog->nodes[prog->node_count++];
    memset(n, 0, sizeof(*n));
    n->op = op;
    if (id) {
        n->id = hu_strdup(&prog->alloc, id);
        if (!n->id) { prog->node_count--; return NULL; }
    }
    return n;
}

/* ── Parse JSON ─────────────────────────────────────────────────────────── */

static hu_hula_op_t parse_op(const char *s) {
    if (!s) return HU_HULA_CALL;
    for (unsigned i = 0; i < HU_HULA_OP_COUNT; i++) {
        if (strcmp(s, op_names[i]) == 0) return (hu_hula_op_t)i;
    }
    return HU_HULA_CALL;
}

static hu_hula_pred_t parse_pred(const char *s) {
    if (!s) return HU_HULA_PRED_SUCCESS;
    for (unsigned i = 0; i <= HU_HULA_PRED_ALWAYS; i++) {
        if (strcmp(s, pred_names[i]) == 0) return (hu_hula_pred_t)i;
    }
    return HU_HULA_PRED_SUCCESS;
}

static hu_hula_node_t *parse_node(hu_hula_program_t *prog, const hu_json_value_t *obj,
                                   int depth);

static void parse_children(hu_hula_program_t *prog, hu_hula_node_t *parent,
                            const hu_json_value_t *arr, int depth) {
    if (!arr || arr->type != HU_JSON_ARRAY) return;
    for (size_t i = 0; i < arr->data.array.len && parent->children_count < HU_HULA_MAX_CHILDREN; i++) {
        hu_hula_node_t *child = parse_node(prog, arr->data.array.items[i], depth + 1);
        if (child) parent->children[parent->children_count++] = child;
    }
}

static hu_hula_node_t *parse_node(hu_hula_program_t *prog, const hu_json_value_t *obj,
                                   int depth) {
    if (!obj || obj->type != HU_JSON_OBJECT || depth > HU_HULA_MAX_DEPTH) return NULL;

    const char *op_str = hu_json_get_string(obj, "op");
    const char *id_str = hu_json_get_string(obj, "id");
    hu_hula_op_t op = parse_op(op_str);

    hu_hula_node_t *n = hu_hula_program_alloc_node(prog, op, id_str);
    if (!n) return NULL;

    const char *s;
    if ((s = hu_json_get_string(obj, "tool")))
        n->tool_name = hu_strdup(&prog->alloc, s);
    if ((s = hu_json_get_string(obj, "description")))
        n->description = hu_strdup(&prog->alloc, s);
    if ((s = hu_json_get_string(obj, "goal"))) {
        n->goal = hu_strdup(&prog->alloc, s);
        n->goal_len = strlen(s);
    }
    if ((s = hu_json_get_string(obj, "model"))) {
        n->delegate_model = hu_strdup(&prog->alloc, s);
        n->delegate_model_len = strlen(s);
    }
    if ((s = hu_json_get_string(obj, "emit_key"))) {
        n->emit_key = hu_strdup(&prog->alloc, s);
        n->emit_key_len = strlen(s);
    }
    if ((s = hu_json_get_string(obj, "emit_value"))) {
        n->emit_value = hu_strdup(&prog->alloc, s);
        n->emit_value_len = strlen(s);
    }
    if ((s = hu_json_get_string(obj, "pred")))
        n->pred = parse_pred(s);
    if ((s = hu_json_get_string(obj, "match"))) {
        n->match_str = hu_strdup(&prog->alloc, s);
        n->match_str_len = strlen(s);
    }

    n->max_iter = (uint32_t)hu_json_get_number(obj, "max_iter", 0);

    /* args: stringify sub-object to JSON string */
    hu_json_value_t *args = hu_json_object_get(obj, "args");
    if (args) {
        char *args_str = NULL;
        size_t args_str_len = 0;
        if (hu_json_stringify(&prog->alloc, args, &args_str, &args_str_len) == HU_OK)
            n->args_json = args_str;
    }

    /* Recurse into children */
    hu_json_value_t *children = hu_json_object_get(obj, "children");
    parse_children(prog, n, children, depth);

    /* BRANCH shorthand: "then" / "else" as children[0] / children[1] */
    if (op == HU_HULA_BRANCH) {
        hu_json_value_t *then_val = hu_json_object_get(obj, "then");
        hu_json_value_t *else_val = hu_json_object_get(obj, "else");
        if (then_val && n->children_count < HU_HULA_MAX_CHILDREN) {
            hu_hula_node_t *t = parse_node(prog, then_val, depth + 1);
            if (t) n->children[n->children_count++] = t;
        }
        if (else_val && n->children_count < HU_HULA_MAX_CHILDREN) {
            hu_hula_node_t *e = parse_node(prog, else_val, depth + 1);
            if (e) n->children[n->children_count++] = e;
        }
    }

    /* LOOP shorthand: "body" as single child */
    if (op == HU_HULA_LOOP) {
        hu_json_value_t *body = hu_json_object_get(obj, "body");
        if (body && n->children_count == 0 && n->children_count < HU_HULA_MAX_CHILDREN) {
            hu_hula_node_t *b = parse_node(prog, body, depth + 1);
            if (b) n->children[n->children_count++] = b;
        }
    }

    return n;
}

hu_error_t hu_hula_parse_json(hu_allocator_t *alloc, const char *json, size_t json_len,
                              hu_hula_program_t *out) {
    if (!alloc || !json || !out) return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *root = NULL;
    hu_error_t err = hu_json_parse(alloc, json, json_len, &root);
    if (err != HU_OK) return err;
    if (root->type != HU_JSON_OBJECT) {
        hu_json_free(alloc, root);
        return HU_ERR_PARSE;
    }

    const char *name = hu_json_get_string(root, "name");
    err = hu_hula_program_init(out, *alloc, name, name ? strlen(name) : 0);
    if (err != HU_OK) { hu_json_free(alloc, root); return err; }

    out->version = (uint32_t)hu_json_get_number(root, "version", HU_HULA_VERSION);

    hu_json_value_t *root_node = hu_json_object_get(root, "root");
    if (root_node && root_node->type == HU_JSON_OBJECT) {
        out->root = parse_node(out, root_node, 0);
    }

    hu_json_free(alloc, root);
    return out->root ? HU_OK : HU_ERR_PARSE;
}

/* ── Serialize to JSON ──────────────────────────────────────────────────── */

static hu_error_t serialize_node(hu_json_buf_t *buf, const hu_hula_node_t *n, int depth);

static hu_error_t serialize_children(hu_json_buf_t *buf, const hu_hula_node_t *n, int depth) {
    hu_error_t err;
    if ((err = hu_json_buf_append_raw(buf, "\"children\":[", 12)) != HU_OK) return err;
    for (size_t i = 0; i < n->children_count; i++) {
        if (i > 0) { if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err; }
        if ((err = serialize_node(buf, n->children[i], depth + 1)) != HU_OK) return err;
    }
    return hu_json_buf_append_raw(buf, "]", 1);
}

static hu_error_t serialize_node(hu_json_buf_t *buf, const hu_hula_node_t *n, int depth) {
    if (!n || depth > HU_HULA_MAX_DEPTH) return HU_ERR_INVALID_ARGUMENT;
    hu_error_t err;
    if ((err = hu_json_buf_append_raw(buf, "{", 1)) != HU_OK) return err;

    if ((err = hu_json_append_key_value(buf, "op", 2, hu_hula_op_name(n->op),
                                         strlen(hu_hula_op_name(n->op)))) != HU_OK) return err;
    if (n->id) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "id", 2, n->id, strlen(n->id))) != HU_OK) return err;
    }
    if (n->tool_name) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "tool", 4, n->tool_name,
                                             strlen(n->tool_name))) != HU_OK) return err;
    }
    if (n->args_json) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key(buf, "args", 4)) != HU_OK) return err;
        if ((err = hu_json_buf_append_raw(buf, n->args_json, strlen(n->args_json))) != HU_OK) return err;
    }
    if (n->description) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "description", 11, n->description,
                                             strlen(n->description))) != HU_OK) return err;
    }
    if (n->goal) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "goal", 4, n->goal, n->goal_len)) != HU_OK)
            return err;
    }
    if (n->delegate_model) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "model", 5, n->delegate_model,
                                             n->delegate_model_len)) != HU_OK) return err;
    }
    if (n->emit_key) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "emit_key", 8, n->emit_key,
                                             n->emit_key_len)) != HU_OK) return err;
    }
    if (n->emit_value) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "emit_value", 10, n->emit_value,
                                             n->emit_value_len)) != HU_OK) return err;
    }
    if (n->op == HU_HULA_BRANCH || n->op == HU_HULA_LOOP) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_value(buf, "pred", 4, hu_hula_pred_name(n->pred),
                                             strlen(hu_hula_pred_name(n->pred)))) != HU_OK)
            return err;
        if (n->match_str) {
            if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
            if ((err = hu_json_append_key_value(buf, "match", 5, n->match_str,
                                                 n->match_str_len)) != HU_OK) return err;
        }
    }
    if (n->op == HU_HULA_LOOP && n->max_iter > 0) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = hu_json_append_key_int(buf, "max_iter", 8, (long long)n->max_iter)) != HU_OK)
            return err;
    }
    if (n->children_count > 0) {
        if ((err = hu_json_buf_append_raw(buf, ",", 1)) != HU_OK) return err;
        if ((err = serialize_children(buf, n, depth)) != HU_OK) return err;
    }

    return hu_json_buf_append_raw(buf, "}", 1);
}

hu_error_t hu_hula_to_json(hu_allocator_t *alloc, const hu_hula_program_t *prog,
                           char **out, size_t *out_len) {
    if (!alloc || !prog || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;

    hu_json_buf_t buf;
    hu_error_t err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK) return err;

    if ((err = hu_json_buf_append_raw(&buf, "{", 1)) != HU_OK) goto fail;
    if (prog->name) {
        if ((err = hu_json_append_key_value(&buf, "name", 4, prog->name,
                                             prog->name_len)) != HU_OK) goto fail;
        if ((err = hu_json_buf_append_raw(&buf, ",", 1)) != HU_OK) goto fail;
    }
    if ((err = hu_json_append_key_int(&buf, "version", 7, prog->version)) != HU_OK) goto fail;
    if (prog->root) {
        if ((err = hu_json_buf_append_raw(&buf, ",", 1)) != HU_OK) goto fail;
        if ((err = hu_json_append_key(&buf, "root", 4)) != HU_OK) goto fail;
        if ((err = serialize_node(&buf, prog->root, 0)) != HU_OK) goto fail;
    }
    if ((err = hu_json_buf_append_raw(&buf, "}", 1)) != HU_OK) goto fail;

    *out = buf.ptr;
    *out_len = buf.len;
    return HU_OK;

fail:
    hu_json_buf_free(&buf);
    return err;
}

/* ── Validate ───────────────────────────────────────────────────────────── */

static void add_diag(hu_hula_validation_t *v, hu_allocator_t *alloc,
                      const hu_hula_node_t *node, const char *msg) {
    if (v->diag_count >= HU_HULA_MAX_DIAGS) return;
    hu_hula_diag_t *d = &v->diags[v->diag_count++];
    d->message = hu_strdup(alloc, msg);
    d->message_len = msg ? strlen(msg) : 0;
    d->node = node;
    v->valid = false;
}

static void validate_node(const hu_hula_node_t *n, hu_allocator_t *alloc,
                           const char *const *tool_names, size_t tool_count,
                           hu_hula_validation_t *v, int depth) {
    if (depth > HU_HULA_MAX_DEPTH) {
        add_diag(v, alloc, n, "exceeds max depth");
        return;
    }
    if (!n->id) add_diag(v, alloc, n, "node missing id");

    switch (n->op) {
    case HU_HULA_CALL:
        if (!n->tool_name)
            add_diag(v, alloc, n, "call node missing tool name");
        else if (tool_names) {
            bool found = false;
            for (size_t i = 0; i < tool_count; i++) {
                if (strcmp(tool_names[i], n->tool_name) == 0) { found = true; break; }
            }
            if (!found) {
                char msg[128];
                (void)snprintf(msg, sizeof(msg), "unknown tool: %.64s", n->tool_name);
                add_diag(v, alloc, n, msg);
            }
        }
        break;
    case HU_HULA_SEQ:
    case HU_HULA_PAR:
        if (n->children_count == 0)
            add_diag(v, alloc, n, "seq/par node has no children");
        break;
    case HU_HULA_BRANCH:
        if (n->children_count < 1)
            add_diag(v, alloc, n, "branch needs at least a then child");
        break;
    case HU_HULA_LOOP:
        if (n->children_count == 0)
            add_diag(v, alloc, n, "loop has no body");
        break;
    case HU_HULA_DELEGATE:
        if (!n->goal || n->goal_len == 0)
            add_diag(v, alloc, n, "delegate node missing goal");
        break;
    case HU_HULA_EMIT:
        if (!n->emit_key || n->emit_key_len == 0)
            add_diag(v, alloc, n, "emit node missing key");
        break;
    default:
        add_diag(v, alloc, n, "unknown opcode");
        break;
    }

    for (size_t i = 0; i < n->children_count; i++) {
        if (n->children[i])
            validate_node(n->children[i], alloc, tool_names, tool_count, v, depth + 1);
    }
}

hu_error_t hu_hula_validate(const hu_hula_program_t *prog, hu_allocator_t *alloc,
                            const char *const *tool_names, size_t tool_count,
                            hu_hula_validation_t *out) {
    if (!prog || !alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->valid = true;

    if (!prog->root) {
        add_diag(out, alloc, NULL, "program has no root node");
        return HU_OK;
    }

    validate_node(prog->root, alloc, tool_names, tool_count, out, 0);
    return HU_OK;
}

void hu_hula_validation_deinit(hu_allocator_t *alloc, hu_hula_validation_t *v) {
    if (!v) return;
    for (size_t i = 0; i < v->diag_count; i++) {
        if (v->diags[i].message)
            hu_str_free(alloc, v->diags[i].message);
    }
    memset(v, 0, sizeof(*v));
}

/* ── Execute ────────────────────────────────────────────────────────────── */

hu_error_t hu_hula_exec_init(hu_hula_exec_t *exec, hu_allocator_t alloc,
                             hu_hula_program_t *prog, hu_tool_t *tools, size_t tools_count) {
    if (!exec || !prog) return HU_ERR_INVALID_ARGUMENT;
    memset(exec, 0, sizeof(*exec));
    exec->alloc = alloc;
    exec->program = prog;
    exec->tools = tools;
    exec->tools_count = tools_count;
    exec->results_count = prog->node_count;
    if (prog->node_count > 0) {
        exec->results = alloc.alloc(alloc.ctx, prog->node_count * sizeof(hu_hula_result_t));
        if (!exec->results) return HU_ERR_OUT_OF_MEMORY;
        memset(exec->results, 0, prog->node_count * sizeof(hu_hula_result_t));
    }
    return HU_OK;
}

static size_t node_index(const hu_hula_program_t *prog, const hu_hula_node_t *n) {
    return (size_t)(n - prog->nodes);
}

static hu_hula_result_t *result_for(hu_hula_exec_t *exec, const hu_hula_node_t *n) {
    size_t idx = node_index(exec->program, n);
    if (idx < exec->results_count) return &exec->results[idx];
    return NULL;
}

static hu_tool_t *find_tool(hu_tool_t *tools, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        const char *tn = tools[i].vtable->name(tools[i].ctx);
        if (tn && strcmp(tn, name) == 0) return &tools[i];
    }
    return NULL;
}

static hu_error_t exec_node(hu_hula_exec_t *exec, hu_hula_node_t *n);

static void set_result(hu_hula_exec_t *exec, hu_hula_node_t *n, hu_hula_status_t status,
                        const char *output, size_t output_len,
                        const char *error, size_t error_len) {
    hu_hula_result_t *r = result_for(exec, n);
    if (!r) return;
    r->status = status;
    if (output && output_len > 0)
        r->output = hu_strndup(&exec->alloc, output, output_len);
    r->output_len = output_len;
    if (error && error_len > 0)
        r->error = hu_strndup(&exec->alloc, error, error_len);
    r->error_len = error_len;
}

static bool eval_pred(hu_hula_exec_t *exec, hu_hula_node_t *n, hu_hula_result_t *last) {
    (void)exec;
    switch (n->pred) {
    case HU_HULA_PRED_SUCCESS:
        return last && last->status == HU_HULA_DONE;
    case HU_HULA_PRED_FAILURE:
        return last && last->status == HU_HULA_FAILED;
    case HU_HULA_PRED_CONTAINS:
        return last && last->output && n->match_str &&
               strstr(last->output, n->match_str) != NULL;
    case HU_HULA_PRED_NOT_CONTAINS:
        return !last || !last->output || !n->match_str ||
               strstr(last->output, n->match_str) == NULL;
    case HU_HULA_PRED_ALWAYS:
        return true;
    }
    return false;
}

/* Get the last completed result for predecessor context */
static hu_hula_result_t *last_result(hu_hula_exec_t *exec) {
    for (size_t i = exec->program->node_count; i > 0; i--) {
        hu_hula_result_t *r = &exec->results[i - 1];
        if (r->status == HU_HULA_DONE || r->status == HU_HULA_FAILED) return r;
    }
    return NULL;
}

static hu_error_t exec_call(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    hu_tool_t *tool = find_tool(exec->tools, exec->tools_count, n->tool_name);
    if (!tool) {
        set_result(exec, n, HU_HULA_FAILED, NULL, 0, "tool not found", 14);
        return HU_OK;
    }

    hu_json_value_t *args = NULL;
    const char *args_src = n->args_json ? n->args_json : "{}";
    hu_error_t err = hu_json_parse(&exec->alloc, args_src, strlen(args_src), &args);
    if (err != HU_OK) {
        set_result(exec, n, HU_HULA_FAILED, NULL, 0, "args parse error", 16);
        return HU_OK;
    }

    hu_tool_result_t tr;
    memset(&tr, 0, sizeof(tr));
    err = tool->vtable->execute(tool->ctx, &exec->alloc, args, &tr);
    hu_json_free(&exec->alloc, args);

    if (err != HU_OK) {
        set_result(exec, n, HU_HULA_FAILED, NULL, 0, "tool execution error", 20);
        return HU_OK;
    }

    if (tr.success)
        set_result(exec, n, HU_HULA_DONE, tr.output, tr.output_len, NULL, 0);
    else
        set_result(exec, n, HU_HULA_FAILED, NULL, 0, tr.error_msg, tr.error_msg_len);

    hu_tool_result_free(&exec->alloc, &tr);
    return HU_OK;
}

static hu_error_t exec_seq(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    for (size_t i = 0; i < n->children_count; i++) {
        hu_error_t err = exec_node(exec, n->children[i]);
        if (err != HU_OK) return err;
        hu_hula_result_t *cr = result_for(exec, n->children[i]);
        if (cr && cr->status == HU_HULA_FAILED) {
            set_result(exec, n, HU_HULA_FAILED, NULL, 0,
                       cr->error, cr->error_len);
            return HU_OK;
        }
    }
    /* Propagate last child's output */
    if (n->children_count > 0) {
        hu_hula_result_t *lr = result_for(exec, n->children[n->children_count - 1]);
        if (lr) set_result(exec, n, HU_HULA_DONE, lr->output, lr->output_len, NULL, 0);
    } else {
        set_result(exec, n, HU_HULA_DONE, NULL, 0, NULL, 0);
    }
    return HU_OK;
}

static hu_error_t exec_par(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    /* Sequential in-process execution (no threading under HU_IS_TEST).
     * Parallel dispatch via pthreads can be added when wired into dispatcher. */
    bool any_failed = false;
    for (size_t i = 0; i < n->children_count; i++) {
        hu_error_t err = exec_node(exec, n->children[i]);
        if (err != HU_OK) return err;
        hu_hula_result_t *cr = result_for(exec, n->children[i]);
        if (cr && cr->status == HU_HULA_FAILED) any_failed = true;
    }
    set_result(exec, n, any_failed ? HU_HULA_FAILED : HU_HULA_DONE, NULL, 0,
               any_failed ? "child failed" : NULL, any_failed ? 12 : 0);
    return HU_OK;
}

static hu_error_t exec_branch(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    hu_hula_result_t *lr = last_result(exec);
    bool cond = eval_pred(exec, n, lr);
    if (cond && n->children_count >= 1) {
        hu_error_t err = exec_node(exec, n->children[0]);
        if (err != HU_OK) return err;
        hu_hula_result_t *cr = result_for(exec, n->children[0]);
        if (cr) set_result(exec, n, cr->status, cr->output, cr->output_len, cr->error, cr->error_len);
    } else if (!cond && n->children_count >= 2) {
        hu_error_t err = exec_node(exec, n->children[1]);
        if (err != HU_OK) return err;
        hu_hula_result_t *cr = result_for(exec, n->children[1]);
        if (cr) set_result(exec, n, cr->status, cr->output, cr->output_len, cr->error, cr->error_len);
    } else {
        set_result(exec, n, HU_HULA_SKIPPED, NULL, 0, NULL, 0);
    }
    return HU_OK;
}

static hu_error_t exec_loop(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    uint32_t max = n->max_iter > 0 ? n->max_iter : 10;
    for (uint32_t i = 0; i < max; i++) {
        hu_hula_result_t *lr = last_result(exec);
        if (n->pred != HU_HULA_PRED_ALWAYS && !eval_pred(exec, n, lr)) break;

        for (size_t c = 0; c < n->children_count; c++) {
            /* Reset child results for re-execution */
            hu_hula_result_t *cr = result_for(exec, n->children[c]);
            if (cr) {
                if (cr->output) hu_str_free(&exec->alloc, cr->output);
                if (cr->error) hu_str_free(&exec->alloc, cr->error);
                memset(cr, 0, sizeof(*cr));
            }
            hu_error_t err = exec_node(exec, n->children[c]);
            if (err != HU_OK) return err;
            cr = result_for(exec, n->children[c]);
            if (cr && cr->status == HU_HULA_FAILED) {
                set_result(exec, n, HU_HULA_FAILED, NULL, 0, cr->error, cr->error_len);
                return HU_OK;
            }
        }
    }
    set_result(exec, n, HU_HULA_DONE, NULL, 0, NULL, 0);
    return HU_OK;
}

static hu_error_t exec_delegate(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    (void)exec;
    /* Delegation requires the spawn system; stub in test/basic mode. */
    set_result(exec, n, HU_HULA_DONE, n->goal, n->goal_len, NULL, 0);
    return HU_OK;
}

static hu_error_t exec_emit(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    /* Resolve $node_id references in emit_value */
    const char *val = n->emit_value ? n->emit_value : "";
    size_t val_len = n->emit_value_len;

    if (n->emit_value && strstr(n->emit_value, "$") != NULL) {
        /* Simple substitution: find $id references */
        for (size_t i = 0; i < exec->program->node_count; i++) {
            hu_hula_node_t *ref = &exec->program->nodes[i];
            if (!ref->id) continue;
            char var[128];
            int vlen = snprintf(var, sizeof(var), "$%s", ref->id);
            if (vlen <= 0) continue;
            if (strstr(n->emit_value, var)) {
                hu_hula_result_t *rr = result_for(exec, ref);
                if (rr && rr->output) { val = rr->output; val_len = rr->output_len; }
                break;
            }
        }
    }

    set_result(exec, n, HU_HULA_DONE, val, val_len, NULL, 0);
    return HU_OK;
}

static hu_error_t exec_node(hu_hula_exec_t *exec, hu_hula_node_t *n) {
    if (!n || exec->halted) return HU_OK;

    hu_hula_result_t *r = result_for(exec, n);
    if (r) r->status = HU_HULA_RUNNING;

    switch (n->op) {
    case HU_HULA_CALL:     return exec_call(exec, n);
    case HU_HULA_SEQ:      return exec_seq(exec, n);
    case HU_HULA_PAR:      return exec_par(exec, n);
    case HU_HULA_BRANCH:   return exec_branch(exec, n);
    case HU_HULA_LOOP:     return exec_loop(exec, n);
    case HU_HULA_DELEGATE: return exec_delegate(exec, n);
    case HU_HULA_EMIT:     return exec_emit(exec, n);
    default:
        set_result(exec, n, HU_HULA_FAILED, NULL, 0, "unknown opcode", 14);
        return HU_OK;
    }
}

hu_error_t hu_hula_exec_run(hu_hula_exec_t *exec) {
    if (!exec || !exec->program || !exec->program->root) return HU_ERR_INVALID_ARGUMENT;
    return exec_node(exec, exec->program->root);
}

const hu_hula_result_t *hu_hula_exec_result(const hu_hula_exec_t *exec, const char *node_id) {
    if (!exec || !node_id) return NULL;
    for (size_t i = 0; i < exec->program->node_count; i++) {
        if (exec->program->nodes[i].id && strcmp(exec->program->nodes[i].id, node_id) == 0)
            return &exec->results[i];
    }
    return NULL;
}

void hu_hula_exec_deinit(hu_hula_exec_t *exec) {
    if (!exec) return;
    for (size_t i = 0; i < exec->results_count; i++) {
        hu_hula_result_t *r = &exec->results[i];
        if (r->output) exec->alloc.free(exec->alloc.ctx, r->output, r->output_len + 1);
        if (r->error) exec->alloc.free(exec->alloc.ctx, r->error, r->error_len + 1);
    }
    if (exec->results)
        exec->alloc.free(exec->alloc.ctx, exec->results,
                         exec->results_count * sizeof(hu_hula_result_t));
    if (exec->halt_reason)
        exec->alloc.free(exec->alloc.ctx, exec->halt_reason, exec->halt_reason_len + 1);
    memset(exec, 0, sizeof(*exec));
}
