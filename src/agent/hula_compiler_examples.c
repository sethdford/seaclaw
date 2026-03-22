#include "human/agent/hula_compiler.h"
#include <string.h>

/* Optional few-shot snippets keyed by coarse domain name (case-sensitive prefix match). */
const char *hu_hula_compiler_examples_for_domain(const char *domain, size_t domain_len,
                                                 size_t *out_len) {
    if (!out_len)
        return NULL;
    *out_len = 0;
    if (!domain || domain_len == 0)
        return NULL;

    static const char finance[] =
        "Example (finance): {\"name\":\"fx\",\"version\":1,\"root\":{\"op\":\"seq\",\"id\":\"r\","
        "\"children\":[{\"op\":\"call\",\"id\":\"q\",\"tool\":\"echo\",\"args\":{\"text\":\"quote "
        "AAPL\"}},{\"op\":\"emit\",\"id\":\"out\",\"emit_key\":\"summary\",\"emit_value\":\"$q\"}"
        "]}}\n";
    static const char security[] =
        "Example (security): {\"name\":\"audit\",\"version\":1,\"root\":{\"op\":\"par\",\"id\":\"p\","
        "\"children\":[{\"op\":\"call\",\"id\":\"a\",\"tool\":\"echo\",\"args\":{\"text\":\"scan\"}},"
        "{\"op\":\"call\",\"id\":\"b\",\"tool\":\"echo\",\"args\":{\"text\":\"report\"}}]}}\n";
    static const char ops[] =
        "Example (ops): {\"name\":\"deploy\",\"version\":1,\"root\":{\"op\":\"seq\",\"id\":\"s\","
        "\"children\":[{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"build\""
        "}},{\"op\":\"branch\",\"id\":\"br\",\"pred\":\"success\",\"then\":{\"op\":\"call\",\"id\":"
        "\"ok\",\"tool\":\"echo\",\"args\":{\"text\":\"ship\"}},\"else\":{\"op\":\"emit\",\"id\":"
        "\"e\",\"emit_key\":\"err\",\"emit_value\":\"rollback\"}}]}}\n";

    if (domain_len >= 4 && memcmp(domain, "fin", 3) == 0) {
        *out_len = sizeof(finance) - 1;
        return finance;
    }
    if (domain_len >= 3 && memcmp(domain, "sec", 3) == 0) {
        *out_len = sizeof(security) - 1;
        return security;
    }
    if (domain_len >= 3 && memcmp(domain, "ops", 3) == 0) {
        *out_len = sizeof(ops) - 1;
        return ops;
    }
    return NULL;
}
