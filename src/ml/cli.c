/* ML CLI subcommands: train, experiment, prepare, status. */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/ml/cli.h"
#include "human/ml/dataloader.h"
#include "human/ml/experiment.h"
#include "human/ml/ml.h"
#include "human/ml/model.h"
#include "human/ml/optimizer.h"
#include "human/ml/prepare.h"
#include "human/ml/tokenizer_ml.h"
#include "human/ml/train.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_int_arg(const char *val, int *out) {
    if (!val || !out)
        return -1;
    char *end = NULL;
    long n = strtol(val, &end, 10);
    if (end == val || *end != '\0' || n < 0)
        return -1;
    *out = (int)n;
    return 0;
}

static const char *get_opt(const char **argv, int argc, int i, const char *opt) {
    if (i + 1 < argc && strcmp(argv[i], opt) == 0)
        return argv[i + 1];
    return NULL;
}

hu_error_t hu_ml_cli_train(hu_allocator_t *alloc, int argc, const char **argv) {
    (void)alloc;
    const char *config_path = NULL;
    for (int i = 1; i < argc; i++) {
        const char *v = get_opt(argv, argc, i, "--config");
        if (v) {
            config_path = v;
            break;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: human ml train [--config <path>] [--help]\n");
            return HU_OK;
        }
    }
#ifdef HU_IS_TEST
    (void)config_path;
    return HU_OK;
#else
    if (!config_path)
        config_path = "config.json";

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open config: %s\n", config_path);
        return HU_ERR_INVALID_ARGUMENT;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024 * 1024) {
        fclose(f);
        fprintf(stderr, "Config file too large or empty: %s\n", config_path);
        return HU_ERR_INVALID_ARGUMENT;
    }
    char *json_buf = (char *)alloc->alloc(alloc->ctx, (size_t)sz + 1);
    if (!json_buf) { fclose(f); return HU_ERR_OUT_OF_MEMORY; }
    if (fread(json_buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); alloc->free(alloc->ctx, json_buf, (size_t)sz + 1);
        return HU_ERR_INVALID_ARGUMENT;
    }
    fclose(f);
    json_buf[sz] = '\0';

    hu_json_value_t *root = hu_json_parse(alloc, json_buf, (size_t)sz);
    alloc->free(alloc->ctx, json_buf, (size_t)sz + 1);
    if (!root) {
        fprintf(stderr, "Invalid JSON in config: %s\n", config_path);
        return HU_ERR_INVALID_ARGUMENT;
    }

    hu_experiment_config_t cfg = hu_experiment_config_default();

    const char *v;
    v = hu_json_get_string(root, "data_dir");
    const char *data_dir = v ? v : ".";

    double dv;
    if ((dv = hu_json_get_number(root, "batch_size")) > 0)
        cfg.training.device_batch_size = (size_t)dv;
    if ((dv = hu_json_get_number(root, "max_steps")) > 0)
        cfg.training.max_steps = (size_t)dv;
    if ((dv = hu_json_get_number(root, "time_budget_secs")) > 0)
        cfg.training.time_budget_secs = (int)dv;

    v = hu_json_get_string(root, "checkpoint_path");
    if (v) cfg.training.checkpoint_path = v;

    hu_model_t model = {0};
    hu_error_t err = hu_gpt_create(alloc, &cfg.gpt, &model);
    if (err != HU_OK) {
        hu_json_free(alloc, root);
        fprintf(stderr, "Model creation failed\n");
        return err;
    }

    hu_ml_optimizer_t optimizer = {0};
    err = hu_muon_adamw_create(alloc, &cfg.optimizer, &optimizer);
    if (err != HU_OK) {
        model.vtable->deinit(&model);
        hu_json_free(alloc, root);
        fprintf(stderr, "Optimizer creation failed\n");
        return err;
    }

    hu_ml_dataloader_t *train_loader = NULL;
    err = hu_ml_dataloader_create(alloc, data_dir, cfg.training.device_batch_size,
                                  cfg.gpt.sequence_len, "train", &train_loader);
    if (err != HU_OK) {
        optimizer.vtable->deinit(&optimizer);
        model.vtable->deinit(&model);
        hu_json_free(alloc, root);
        fprintf(stderr, "Dataloader creation failed for %s\n", data_dir);
        return err;
    }

    printf("Training: batch_size=%zu, max_steps=%zu, data=%s\n",
           cfg.training.device_batch_size, cfg.training.max_steps, data_dir);

    hu_ml_train_result_t result = {0};
    err = hu_ml_train(alloc, &model, &optimizer, train_loader, NULL,
                      &cfg.training, NULL, 0, &result);

    printf("Training %s: %zu steps, %.2f bpb, %.1fs\n",
           err == HU_OK ? "complete" : "failed",
           result.num_steps, result.val_bpb, result.training_seconds);

    hu_ml_dataloader_deinit(train_loader);
    optimizer.vtable->deinit(&optimizer);
    model.vtable->deinit(&model);
    hu_json_free(alloc, root);
    return err;
#endif
}

hu_error_t hu_ml_cli_experiment(hu_allocator_t *alloc, int argc, const char **argv) {
    const char *config_path = NULL;
    const char *data_dir = NULL;
    int max_iterations = 10;
    for (int i = 1; i < argc; i++) {
        const char *v = get_opt(argv, argc, i, "--config");
        if (v)
            config_path = v;
        v = get_opt(argv, argc, i, "--data-dir");
        if (v)
            data_dir = v;
        v = get_opt(argv, argc, i, "--max-iterations");
        if (v && parse_int_arg(v, &max_iterations) != 0) {
            fprintf(stderr, "Invalid --max-iterations: %s\n", v);
            return HU_ERR_INVALID_ARGUMENT;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: human experiment [--config <path>] [--max-iterations <N>] "
                   "[--data-dir <path>] [--help]\n");
            return HU_OK;
        }
    }
#ifdef HU_IS_TEST
    (void)alloc;
    (void)config_path;
    (void)data_dir;
    (void)max_iterations;
    return HU_OK;
#else
    if (!data_dir)
        data_dir = ".";
    hu_experiment_loop_config_t loop_cfg = {0};
    loop_cfg.max_iterations = max_iterations;
    loop_cfg.base_config = hu_experiment_config_default();
    loop_cfg.data_dir = data_dir;
    loop_cfg.convergence_threshold = 0.0;
    (void)config_path;
    return hu_experiment_loop(alloc, &loop_cfg, NULL, NULL);
#endif
}

hu_error_t hu_ml_cli_prepare(hu_allocator_t *alloc, int argc, const char **argv) {
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    int vocab_size = 8192;
    for (int i = 1; i < argc; i++) {
        const char *v = get_opt(argv, argc, i, "--input");
        if (v)
            input_dir = v;
        v = get_opt(argv, argc, i, "--output");
        if (v)
            output_dir = v;
        v = get_opt(argv, argc, i, "--vocab-size");
        if (v && parse_int_arg(v, &vocab_size) != 0) {
            fprintf(stderr, "Invalid --vocab-size: %s\n", v);
            return HU_ERR_INVALID_ARGUMENT;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: human ml prepare [--input <dir>] [--output <dir>] "
                   "[--vocab-size <N>] [--help]\n");
            return HU_OK;
        }
    }
#ifdef HU_IS_TEST
    (void)alloc;
    (void)input_dir;
    (void)output_dir;
    (void)vocab_size;
    return HU_OK;
#else
    if (!input_dir || !output_dir) {
        fprintf(stderr, "prepare requires --input and --output\n");
        return HU_ERR_INVALID_ARGUMENT;
    }
    hu_bpe_tokenizer_t *tok = NULL;
    hu_error_t err = hu_bpe_tokenizer_create(alloc, &tok);
    if (err != HU_OK)
        return err;
    /* Use byte-level tokenizer; optional BPE training would need corpus scan */
    (void)vocab_size;
    err = hu_ml_prepare_tokenize_dir(alloc, tok, input_dir, output_dir);
    hu_bpe_tokenizer_deinit(tok);
    return err;
#endif
}

hu_error_t hu_ml_cli_status(hu_allocator_t *alloc, int argc, const char **argv) {
    (void)alloc;
    (void)argc;
    (void)argv;
#ifdef HU_IS_TEST
    printf("No experiments found\n");
    return HU_OK;
#else
    const char *path = "results.tsv";
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No experiments found\n");
        return HU_OK;
    }
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '\0' && line[0] != '#') {
            printf("%s", line);
            count++;
        }
    }
    fclose(f);
    if (count == 0)
        printf("No experiments found\n");
    return HU_OK;
#endif
}
