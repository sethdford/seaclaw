# Feynman Technique

Use plain-language explanation as a **proof of understanding**: gaps in a simple story reveal gaps in your model. This is not dumbing down—it is stress-testing whether you can operate the idea without jargon.

## When to Use
- Before recommending a plan, architecture, or fix; when teaching; when the user is stuck “knowing the words” but not the mechanism.

## Workflow
1. **Choose one concept** (single claim or mechanism, not a whole field).
2. **Explain it as if to a curious 12-year-old**: steps, cause→effect, one concrete example.
3. **Mark breakpoints**: where you hand-wave, use a label without a process, or cannot predict what happens if a variable changes.
4. **Repair**: define terms, add a constraint, draw a boundary, or swap in an analogy—then re-run the explanation until breakpoints shrink.

If you cannot explain it simply after repair, **say what is unknown** and what evidence would resolve it. Before giving advice, run this on **your own** reasoning first.

## Examples
**Example 1:** “HTTPS is secure” → explain TLS handshake + who you trust (CAs) + what it does *not* protect (phishing); breakpoint often at “who verifies the server.”

**Example 2:** “We should shard the DB” → explain hot keys, cross-shard queries, and ops cost; breakpoint often confuses throughput with latency or ignores consistency mode.
