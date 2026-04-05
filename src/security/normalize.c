#include "human/security/normalize.h"
#include <string.h>

hu_error_t hu_normalize_confusables(const char *input, size_t input_len,
                                     char *out, size_t out_cap, size_t *out_len) {
    if (!input || !out || !out_len || out_cap == 0)
        return HU_ERR_INVALID_ARGUMENT;

    size_t j = 0;
    size_t i = 0;
    while (i < input_len && j < out_cap - 1) {
        unsigned char c = (unsigned char)input[i];

        /* Strip zero-width characters (UTF-8 sequences) */
        if (c == 0xE2 && i + 2 < input_len) {
            unsigned char b1 = (unsigned char)input[i + 1];
            unsigned char b2 = (unsigned char)input[i + 2];
            /* U+200B (e2 80 8b), U+200C (e2 80 8c), U+200D (e2 80 8d) */
            if (b1 == 0x80 && (b2 == 0x8B || b2 == 0x8C || b2 == 0x8D)) {
                i += 3;
                continue;
            }
        }
        if (c == 0xEF && i + 2 < input_len) {
            unsigned char b1 = (unsigned char)input[i + 1];
            unsigned char b2 = (unsigned char)input[i + 2];
            /* U+FEFF BOM (ef bb bf) */
            if (b1 == 0xBB && b2 == 0xBF) {
                i += 3;
                continue;
            }
        }

        /* Collapse whitespace (space, tab, etc.) - skip spaces entirely so "k i l l" → "kill" */
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            i++;
            continue;
        }

        /* Map common homoglyphs to ASCII equivalent */
        char mapped;
        switch (c) {
        case '0': mapped = 'o'; break;
        case '1': mapped = 'i'; break;
        case '3': mapped = 'e'; break;
        case '4': mapped = 'a'; break;
        case '5': mapped = 's'; break;
        case '7': mapped = 't'; break;
        case '@': mapped = 'a'; break;
        case '$': mapped = 's'; break;
        case '!': mapped = 'i'; break;
        default:
            /* ASCII lowercase fold */
            if (c >= 'A' && c <= 'Z')
                mapped = (char)(c + 32);
            else
                mapped = (char)c;
            break;
        }
        out[j++] = mapped;
        i++;
    }
    out[j] = '\0';
    *out_len = j;
    return HU_OK;
}
