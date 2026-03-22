# Metacognition analytics (`metacog_history`)

The cognition database (`~/.human/cognition.db`, table `metacog_history`) stores one row per assistant completion when metacognition is enabled. Useful columns:

| Column | Meaning |
|--------|---------|
| `trace_id` | Correlates with the agent turn; follow-up user message may set `outcome_proxy`. |
| `iteration` | Tool-loop iteration when the snapshot was taken. |
| `confidence`, `coherence`, `repetition`, `stuck_score`, `satisfaction_proxy`, `trajectory_confidence` | Heuristic signals at insert time. |
| `risk_score` | Calibrated composite in \([0,1]\). |
| `logprob_mean` | Mean completion-token logprob when the provider returned logprobs; `-1` if unset. |
| `prompt_tokens`, `completion_tokens` | Usage for that generation. |
| `regen_applied` | `1` if at least one metacognition regeneration ran. |
| `outcome_proxy` | Filled on the **next** user turn when follow-up labeling runs. |

## Example: high-risk rows with poor follow-up

```sql
SELECT trace_id, risk_score, outcome_proxy, action, difficulty, timestamp
FROM metacog_history
WHERE risk_score > 0.65
  AND (outcome_proxy IS NULL OR outcome_proxy < 0)
ORDER BY id DESC
LIMIT 50;
```

## Example: logprobs vs outcome (when `HUMAN_METACOG_LOGPROBS` was on)

```sql
SELECT AVG(logprob_mean) AS avg_lp, AVG(outcome_proxy) AS avg_ox, COUNT(*) AS n
FROM metacog_history
WHERE logprob_mean IS NOT NULL AND logprob_mean > -0.5;
```

Run with `sqlite3 ~/.human/cognition.db`. Under tests, cognition DB may be `:memory:` only.
