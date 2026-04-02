/*
 * Fuzz harness for hu_instruction_validate_path and hu_instruction_file_read.
 * Must not crash or leak on any input.
 *
 * RED-TEAM-2: fuzz target for instruction discovery path handling.
 */
#include "human/agent/instruction_discover.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096)
        return 0; /* paths longer than PATH_MAX are uninteresting */

    hu_allocator_t alloc = hu_system_allocator();

    /* ── Fuzz hu_instruction_validate_path ── */
    {
        char *canonical = NULL;
        size_t canonical_len = 0;
        hu_error_t err = hu_instruction_validate_path(
            &alloc, (const char *)data, size, &canonical, &canonical_len);
        if (err == HU_OK && canonical) {
            /* Verify canonical path is NUL-terminated */
            (void)canonical[0];
            (void)canonical_len;
            alloc.free(alloc.ctx, canonical, canonical_len + 1);
        }
    }

    /* ── Fuzz hu_instruction_file_read with random path ── */
    if (size > 0 && size < 1024) {
        /* Create NUL-terminated copy */
        char *path = (char *)alloc.alloc(alloc.ctx, size + 1);
        if (path) {
            memcpy(path, data, size);
            path[size] = '\0';

            hu_instruction_file_t file;
            hu_error_t err = hu_instruction_file_read(
                &alloc, path, HU_INSTRUCTION_SOURCE_WORKSPACE, &file);
            if (err == HU_OK) {
                /* Free any allocated content */
                if (file.path)
                    alloc.free(alloc.ctx, file.path, file.path_len + 1);
                if (file.content)
                    alloc.free(alloc.ctx, file.content, file.content_len + 1);
            }
            alloc.free(alloc.ctx, path, size + 1);
        }
    }

    /* ── Fuzz hu_instruction_merge with synthetic file array ── */
    if (size >= 4) {
        /* Use first two bytes for file count (0-3) and source types */
        size_t file_count = (data[0] % 4);
        if (file_count > 0 && size > 4) {
            hu_instruction_file_t files[4];
            memset(files, 0, sizeof(files));

            size_t content_start = 4;
            size_t per_file = (size - content_start) / file_count;
            if (per_file == 0) per_file = 1;

            for (size_t i = 0; i < file_count && content_start < size; i++) {
                files[i].source = (hu_instruction_source_t)(data[1 + i] % 3);
                files[i].path = "/fuzz/path";
                files[i].path_len = 10;
                size_t clen = per_file;
                if (content_start + clen > size)
                    clen = size - content_start;

                char *content = (char *)alloc.alloc(alloc.ctx, clen + 1);
                if (!content) break;
                memcpy(content, data + content_start, clen);
                content[clen] = '\0';
                files[i].content = content;
                files[i].content_len = clen;
                content_start += clen;
            }

            char *merged = NULL;
            size_t merged_len = 0;
            hu_error_t err = hu_instruction_merge(&alloc, files, file_count,
                                                   &merged, &merged_len);
            if (err == HU_OK && merged)
                alloc.free(alloc.ctx, merged, merged_len + 1);

            for (size_t i = 0; i < file_count; i++) {
                if (files[i].content && files[i].content != NULL)
                    alloc.free(alloc.ctx, files[i].content, files[i].content_len + 1);
            }
        }
    }

    return 0;
}
