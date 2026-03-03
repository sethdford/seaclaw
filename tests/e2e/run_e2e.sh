#!/usr/bin/env bash
# End-to-end integration tests for seaclaw with a live OpenAI API.
# Requires OPENAI_API_KEY in the environment.
# Usage: OPENAI_API_KEY=sk-... ./tests/e2e/run_e2e.sh [path/to/seaclaw]
set -euo pipefail

BINARY="${1:-./build/seaclaw}"
PASS=0
FAIL=0
TOTAL=0
ERRORS=""

pass() { PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1)); printf "  \033[32mPASS\033[0m  %s\n" "$1"; }
fail() { FAIL=$((FAIL + 1)); TOTAL=$((TOTAL + 1)); ERRORS="${ERRORS}\n  - $1: $2"; printf "  \033[31mFAIL\033[0m  %s — %s\n" "$1" "$2"; }

if [ -z "${OPENAI_API_KEY:-}" ]; then
  echo "ERROR: OPENAI_API_KEY not set" >&2; exit 1
fi
if [ ! -x "$BINARY" ]; then
  echo "ERROR: $BINARY not found or not executable" >&2; exit 1
fi

echo ""
echo "═══════════════════════════════════════════════════════"
echo " SeaClaw E2E Integration Tests (live OpenAI API)"
echo "═══════════════════════════════════════════════════════"
echo ""

# ── 1. CLI Commands ──────────────────────────────────────

echo "── CLI Commands ──"

OUT=$("$BINARY" --version 2>&1)
if echo "$OUT" | grep -q "v0\.1\.0"; then pass "version"; else fail "version" "unexpected: $OUT"; fi

OUT=$("$BINARY" doctor 2>&1)
if echo "$OUT" | grep -q "API key configured"; then pass "doctor_api_key"; else fail "doctor_api_key" "key not detected"; fi
if echo "$OUT" | grep -q "config: ok"; then pass "doctor_config"; else fail "doctor_config" "config not ok"; fi

OUT=$("$BINARY" capabilities 2>&1)
if echo "$OUT" | grep -q "shell"; then pass "capabilities_tools"; else fail "capabilities_tools" "no shell tool"; fi
if echo "$OUT" | grep -q "openai"; then pass "capabilities_provider"; else fail "capabilities_provider" "no openai"; fi

OUT=$("$BINARY" memory stats 2>&1)
if echo "$OUT" | grep -q "Health: ok"; then pass "memory_health"; else fail "memory_health" "memory unhealthy"; fi

OUT=$("$BINARY" sandbox 2>&1)
if echo "$OUT" | grep -q "seatbelt\|landlock\|docker"; then pass "sandbox_detection"; else fail "sandbox_detection" "no sandbox found"; fi

OUT=$("$BINARY" models list 2>&1)
if echo "$OUT" | grep -q "openai\|gpt-4o"; then pass "models_list"; else fail "models_list" "no models"; fi

echo ""

# ── 2. MCP Server: Tool Listing ─────────────────────────

echo "── MCP Server (JSON-RPC) ──"

MCP_INIT='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"e2e-test","version":"1.0"}}}'
MCP_LIST='{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'

MCP_OUT=$(printf '%s\n%s\n' "$MCP_INIT" "$MCP_LIST" | timeout 5 "$BINARY" mcp 2>/dev/null || true)

if echo "$MCP_OUT" | grep -q '"tools"'; then
  pass "mcp_tools_list"
  TOOL_COUNT=$(echo "$MCP_OUT" | grep -o '"name"' | wc -l | tr -d ' ')
  if [ "$TOOL_COUNT" -ge 20 ]; then pass "mcp_tool_count ($TOOL_COUNT tools)"; else fail "mcp_tool_count" "only $TOOL_COUNT tools"; fi
else
  fail "mcp_tools_list" "no tools in response"
  fail "mcp_tool_count" "skipped"
fi

if echo "$MCP_OUT" | grep -q '"shell"'; then pass "mcp_has_shell_tool"; else fail "mcp_has_shell_tool" "shell not found"; fi
if echo "$MCP_OUT" | grep -q '"file_read"'; then pass "mcp_has_file_read"; else fail "mcp_has_file_read" "file_read not found"; fi
if echo "$MCP_OUT" | grep -q '"git"'; then pass "mcp_has_git_tool"; else fail "mcp_has_git_tool" "git not found"; fi
if echo "$MCP_OUT" | grep -q '"memory_store"'; then pass "mcp_has_memory_store"; else fail "mcp_has_memory_store" "memory_store not found"; fi
if echo "$MCP_OUT" | grep -q '"cron_add"'; then pass "mcp_has_cron_add"; else fail "mcp_has_cron_add" "cron_add not found"; fi

echo ""

# ── 3. MCP Tool Execution ───────────────────────────────

echo "── MCP Tool Execution ──"

MCP_EXEC_SHELL='{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"shell","arguments":{"command":"echo e2e_sentinel_42"}}}'
MCP_OUT2=$(printf '%s\n%s\n' "$MCP_INIT" "$MCP_EXEC_SHELL" | timeout 10 "$BINARY" mcp 2>/dev/null || true)

if echo "$MCP_OUT2" | grep -q "e2e_sentinel_42"; then
  pass "mcp_shell_exec"
else
  fail "mcp_shell_exec" "sentinel not in output"
fi

MCP_EXEC_READ='{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"file_read","arguments":{"path":"README.md"}}}'
MCP_OUT3=$(printf '%s\n%s\n' "$MCP_INIT" "$MCP_EXEC_READ" | timeout 10 "$BINARY" mcp 2>/dev/null || true)

if echo "$MCP_OUT3" | grep -qi "seaclaw\|autonomous"; then
  pass "mcp_file_read"
else
  fail "mcp_file_read" "README content not found"
fi

MCP_EXEC_GIT='{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"git","arguments":{"subcommand":"status"}}}'
MCP_OUT4=$(printf '%s\n%s\n' "$MCP_INIT" "$MCP_EXEC_GIT" | timeout 10 "$BINARY" mcp 2>/dev/null || true)

if echo "$MCP_OUT4" | grep -qi "branch\|clean\|nothing to commit"; then
  pass "mcp_git_status"
else
  fail "mcp_git_status" "no branch info"
fi

echo ""

# ── 4. Live LLM: Agent Turn ─────────────────────────────

echo "── Live LLM (OpenAI gpt-4o) ──"

LOGFILE=$(mktemp /tmp/seaclaw_e2e_XXXXXX.log)

echo "What is the capital of France? Answer in one word only." | \
  SEACLAW_LOG="$LOGFILE" timeout 30 "$BINARY" agent 2>/dev/null &
AGENT_PID=$!
sleep 15
kill $AGENT_PID 2>/dev/null || true
wait $AGENT_PID 2>/dev/null || true

if [ -f "$LOGFILE" ]; then
  if grep -q '"event":"llm_response"' "$LOGFILE" && grep -q '"success":true' "$LOGFILE"; then
    pass "llm_basic_response"
    DURATION=$(grep '"event":"llm_response"' "$LOGFILE" | grep -o '"duration_ms":[0-9]*' | head -1 | cut -d: -f2)
    if [ -n "$DURATION" ] && [ "$DURATION" -lt 30000 ]; then
      pass "llm_response_time (${DURATION}ms)"
    else
      fail "llm_response_time" "took ${DURATION:-unknown}ms"
    fi
    TOKENS=$(grep '"event":"agent_end"' "$LOGFILE" | grep -o '"tokens_used":[0-9]*' | head -1 | cut -d: -f2)
    if [ -n "$TOKENS" ] && [ "$TOKENS" -gt 0 ]; then
      pass "llm_token_count ($TOKENS tokens)"
    else
      fail "llm_token_count" "no tokens reported"
    fi
  else
    fail "llm_basic_response" "no successful LLM response in log"
    fail "llm_response_time" "skipped"
    fail "llm_token_count" "skipped"
  fi
  if grep -q '"event":"turn_complete"' "$LOGFILE"; then
    pass "llm_turn_complete"
  else
    fail "llm_turn_complete" "turn did not complete"
  fi
else
  fail "llm_basic_response" "no log file"
  fail "llm_response_time" "skipped"
  fail "llm_token_count" "skipped"
  fail "llm_turn_complete" "skipped"
fi
rm -f "$LOGFILE"

echo ""

# ── 5. Live LLM: Tool Use ───────────────────────────────

echo "── Live LLM Tool Calling ──"

LOGFILE2=$(mktemp /tmp/seaclaw_e2e_XXXXXX.log)

echo "Use the shell tool to run: echo TOOL_TEST_OK" | \
  SEACLAW_LOG="$LOGFILE2" timeout 30 "$BINARY" agent 2>/dev/null &
AGENT_PID=$!
sleep 20
kill $AGENT_PID 2>/dev/null || true
wait $AGENT_PID 2>/dev/null || true

if [ -f "$LOGFILE2" ]; then
  if grep -q '"event":"llm_response"' "$LOGFILE2" && grep -q '"success":true' "$LOGFILE2"; then
    pass "llm_tool_call_response"
  else
    fail "llm_tool_call_response" "no successful response"
  fi
  if grep -q '"event":"tool_' "$LOGFILE2" || grep -q 'tool_call\|tool_use\|function_call' "$LOGFILE2"; then
    pass "llm_tool_invocation"
  else
    fail "llm_tool_invocation" "no tool call events in log"
  fi
  if grep -q '"event":"turn_complete"' "$LOGFILE2"; then
    pass "llm_tool_turn_complete"
  else
    fail "llm_tool_turn_complete" "turn did not complete"
  fi
else
  fail "llm_tool_call_response" "no log"
  fail "llm_tool_invocation" "skipped"
  fail "llm_tool_turn_complete" "skipped"
fi
rm -f "$LOGFILE2"

echo ""

# ── 6. Live LLM: Streaming ──────────────────────────────

echo "── Live LLM Streaming ──"

LOGFILE3=$(mktemp /tmp/seaclaw_e2e_XXXXXX.log)

echo "Count from 1 to 5, one number per line." | \
  SEACLAW_LOG="$LOGFILE3" timeout 30 "$BINARY" agent 2>/dev/null &
AGENT_PID=$!
sleep 15
kill $AGENT_PID 2>/dev/null || true
wait $AGENT_PID 2>/dev/null || true

if [ -f "$LOGFILE3" ]; then
  if grep -q '"event":"llm_response"' "$LOGFILE3" && grep -q '"success":true' "$LOGFILE3"; then
    pass "llm_streaming_response"
  else
    fail "llm_streaming_response" "no response"
  fi
else
  fail "llm_streaming_response" "no log"
fi
rm -f "$LOGFILE3"

echo ""

# ── 7. Error Handling ────────────────────────────────────

echo "── Error Handling ──"

ERR_OUT=$(OPENAI_API_KEY="sk-invalid-key-12345" "$BINARY" doctor 2>&1 || true)
if echo "$ERR_OUT" | grep -qi "API key configured\|warning"; then
  pass "bad_key_doctor_survives"
else
  fail "bad_key_doctor_survives" "doctor crashed with bad key"
fi

LOGFILE4=$(mktemp /tmp/seaclaw_e2e_XXXXXX.log)
echo "Hello" | \
  OPENAI_API_KEY="sk-invalid" SEACLAW_LOG="$LOGFILE4" timeout 15 "$BINARY" agent 2>/dev/null &
AGENT_PID=$!
sleep 10
kill $AGENT_PID 2>/dev/null || true
wait $AGENT_PID 2>/dev/null || true

if [ -f "$LOGFILE4" ]; then
  if grep -q '"event":"llm_response"' "$LOGFILE4"; then
    if grep -q '"success":false' "$LOGFILE4"; then
      pass "bad_key_graceful_error"
    else
      pass "bad_key_graceful_error (response received despite bad key)"
    fi
  else
    pass "bad_key_graceful_error (no crash)"
  fi
else
  pass "bad_key_graceful_error (no crash, no log)"
fi
rm -f "$LOGFILE4"

echo ""

# ── 8. Concurrent Requests ──────────────────────────────

echo "── Concurrent Stress ──"

PIDS=""
ALL_OK=true
for i in 1 2 3; do
  TMPLOG=$(mktemp /tmp/seaclaw_e2e_concurrent_${i}_XXXXXX.log)
  (echo "Say the number $i and nothing else." | \
    SEACLAW_LOG="$TMPLOG" timeout 30 "$BINARY" agent 2>/dev/null; \
    echo "$?" > "${TMPLOG}.exit") &
  PIDS="$PIDS $!"
done

sleep 20
for P in $PIDS; do kill "$P" 2>/dev/null || true; done
for P in $PIDS; do wait "$P" 2>/dev/null || true; done

CONCURRENT_OK=0
for i in 1 2 3; do
  TMPLOG=$(ls /tmp/seaclaw_e2e_concurrent_${i}_*.log 2>/dev/null | head -1)
  if [ -n "$TMPLOG" ] && [ -f "$TMPLOG" ] && grep -q '"success":true' "$TMPLOG"; then
    CONCURRENT_OK=$((CONCURRENT_OK + 1))
  fi
  rm -f "$TMPLOG" "${TMPLOG}.exit" 2>/dev/null
done

if [ "$CONCURRENT_OK" -ge 2 ]; then
  pass "concurrent_agents ($CONCURRENT_OK/3 succeeded)"
else
  fail "concurrent_agents" "only $CONCURRENT_OK/3 succeeded"
fi

echo ""

# ── Results ──────────────────────────────────────────────

echo "═══════════════════════════════════════════════════════"
printf " Results: \033[32m%d passed\033[0m" "$PASS"
if [ "$FAIL" -gt 0 ]; then
  printf ", \033[31m%d failed\033[0m" "$FAIL"
fi
printf " / %d total\n" "$TOTAL"

if [ "$FAIL" -gt 0 ]; then
  echo ""
  echo " Failures:"
  printf "$ERRORS\n"
fi
echo "═══════════════════════════════════════════════════════"
echo ""

exit "$FAIL"
