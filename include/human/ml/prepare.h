#ifndef HU_ML_PREPARE_H
#define HU_ML_PREPARE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/ml/tokenizer_ml.h"
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Data preparation utilities
 * ────────────────────────────────────────────────────────────────────────── */

hu_error_t hu_ml_prepare_tokenize_file(hu_allocator_t *alloc, hu_bpe_tokenizer_t *tok,
                                       const char *input_path, const char *output_path);

hu_error_t hu_ml_prepare_tokenize_dir(hu_allocator_t *alloc, hu_bpe_tokenizer_t *tok,
                                      const char *input_dir, const char *output_dir);

hu_error_t hu_ml_prepare_token_bytes(hu_allocator_t *alloc, hu_bpe_tokenizer_t *tok,
                                     int32_t **token_bytes_out, size_t *count);

/* Prepare conversation data from chat.db + memory.db into tokenized .bin files.
 * Reads iMessage conversations and memory entries, formats as turn-delimited text,
 * tokenizes via BPE, and writes train/val .bin splits to output_dir. */
hu_error_t hu_ml_prepare_conversations(hu_allocator_t *alloc, hu_bpe_tokenizer_t *tok,
                                       const char *chat_db_path, const char *memory_db_path,
                                       const char *output_dir, size_t *messages_processed);

#endif
