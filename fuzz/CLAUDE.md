# fuzz/ — LibFuzzer Harnesses

Fuzz testing for security-critical parsers and input handling.

## Commands

```bash
cmake -B build-fuzz -DHU_ENABLE_FUZZ=ON -DCMAKE_C_COMPILER=clang
cmake --build build-fuzz -j$(nproc)
./build-fuzz/fuzz_json_parse corpus/json -max_total_time=60
```

## Rules

- Harnesses must not crash or trigger ASan/UBSan errors
- Seed corpora live in `fuzz/corpus/<harness>/`
- Each harness targets one parser or input handler
- Keep harnesses small and focused — one entry point per file
- New harnesses must be added to both `CMakeLists.txt` and CI (`ci.yml` fuzz job)

## Existing Harnesses (31)

| Harness                          | Target                                        |
| -------------------------------- | --------------------------------------------- |
| `fuzz_adaptive_rag`              | Adaptive RAG feature extraction + selection   |
| `fuzz_base64`                    | Base64 codec                                  |
| `fuzz_compaction_overflow`       | Structured compaction summary + metadata      |
| `fuzz_config`                    | Config parser                                 |
| `fuzz_config_load`               | Config loader                                 |
| `fuzz_contact_phone`             | Contact phone number normalization            |
| `fuzz_control_protocol`          | Gateway control protocol parser               |
| `fuzz_ensemble_rerank`           | Ensemble rerank digit parsing                 |
| `fuzz_fast_capture`              | Fast capture entity/emotion/topic extraction  |
| `fuzz_graph`                     | Graph entity/relation upsert                  |
| `fuzz_hook_injection`            | Hook shell escaping + pipeline execution      |
| `fuzz_http_parse`                | HTTP request parser                           |
| `fuzz_imessage_attributed_body`  | NSKeyedArchiver attributed body extraction    |
| `fuzz_instruction_discovery`     | Instruction path validation + file read       |
| `fuzz_json`                      | JSON lexer/tokenizer                          |
| `fuzz_json_parse`                | JSON parser                                   |
| `fuzz_mcp_json_rpc`              | MCP tool name parsing + server lookup         |
| `fuzz_memory_tiers`              | Memory tiers prompt build + auto-tiering      |
| `fuzz_openai_compat`             | OpenAI-compatible API parser                  |
| `fuzz_persona_json`              | Persona JSON parser                           |
| `fuzz_persona_parse`             | Persona JSON parsing (alias)                  |
| `fuzz_prm`                       | PRM step splitting and scoring                |
| `fuzz_prompt_injection`          | Prompt injection detection                    |
| `fuzz_query_param`               | Query parameter parser                        |
| `fuzz_self_rag`                  | Self-RAG retrieval decision + relevance       |
| `fuzz_session_deserialization`   | Session persistence JSON load/save            |
| `fuzz_sse`                       | SSE stream parser                             |
| `fuzz_tool_params`               | Tool parameter parser                         |
| `fuzz_twilio_webhook`            | Twilio webhook handler                        |
| `fuzz_url_encode`                | URL encoder                                   |
| `fuzz_visual_grounding`          | Visual grounding JSON response parsing        |
