#ifndef SC_TOOLS_SCHEMA_COMMON_H
#define SC_TOOLS_SCHEMA_COMMON_H

/*
 * Shared JSON schema fragments for tool parameter definitions.
 * Using shared macros ensures the compiler merges identical string literals
 * and reduces __cstring section bloat.
 */

#define SC_SCHEMA_EMPTY \
    "{\"type\":\"object\",\"properties\":{},\"required\":[]}"

#define SC_SCHEMA_PATH_ONLY \
    "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"" \
    "}},\"required\":[\"path\"]}"

#define SC_SCHEMA_PATH_CONTENT \
    "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"" \
    "},\"content\":{\"type\":\"string\"}},\"required\":[\"path\"," \
    "\"content\"]}"

#define SC_SCHEMA_KEY_ONLY \
    "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\"" \
    "}},\"required\":[\"key\"]}"

#define SC_SCHEMA_ID_ONLY \
    "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"" \
    "}},\"required\":[\"id\"]}"

#define SC_SCHEMA_URL_ONLY \
    "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\"" \
    "}},\"required\":[\"url\"]}"

#define SC_SCHEMA_QUERY_ONLY \
    "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"" \
    "}},\"required\":[\"query\"]}"

#endif /* SC_TOOLS_SCHEMA_COMMON_H */
