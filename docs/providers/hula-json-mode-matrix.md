# HuLa compiler — JSON / `json_object` mode matrix

The HuLa compiler calls the active provider `chat` with `response_format` set to `json_object` (OpenAI-style). The generic adapter builds this in `src/providers/compatible.c` as:

```json
{ "response_format": { "type": "json_object" } }
```

| Class | Examples | Typical `json_object` support |
| ----- | -------- | ----------------------------- |
| OpenAI official | api.openai.com | Supported for compatible models |
| Azure OpenAI | Configured base URL | Same schema as OpenAI |
| Anthropic via proxy | Various | Depends on proxy; native Anthropic path differs |
| Local OpenAI-compat | Ollama, LM Studio, vLLM | Varies; some need `format: json` elsewhere |
| Aggregators | OpenRouter, Together | Check model card; may ignore `response_format` |

**Operational note:** When `json_object` is ignored, the compiler repair loop (`hula_compiler.c`) retries with validation diagnostics; persistent failure usually means the endpoint does not constrain output to JSON.
