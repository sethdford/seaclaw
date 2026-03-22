#!/usr/bin/env node
/**
 * One-shot WebSocket client: connect → optional connect RPC → voice.transcribe (base64 WAV).
 * Matches ui/src/gateway.ts envelope: { type, id, method, params } / { type, id, ok, payload }.
 *
 * Usage: node scripts/e2e-voice-stt-gateway.mjs <path-to.wav> [ws-url]
 * Env:   E2E_GATEWAY_WS — default ws://127.0.0.1:3009/ws
 *
 * Requires Node >= 18 with global WebSocket (Node 22+).
 */
import { readFileSync } from "node:fs";
import { basename } from "node:path";

const wavPath = process.argv[2];
const wsUrl = process.argv[3] || process.env.E2E_GATEWAY_WS || "ws://127.0.0.1:3009/ws";

if (!wavPath) {
  console.error(`Usage: node ${basename(process.argv[1])} <wav-file> [ws-url]`);
  process.exit(2);
}

const audio = readFileSync(wavPath);
const b64 = audio.toString("base64");

const pending = new Map();
let seq = 0;

function sendRpc(ws, method, params, timeoutMs) {
  return new Promise((resolve, reject) => {
    const id = `e2e-${Date.now()}-${++seq}`;
    const t = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`timeout: ${method} (${timeoutMs}ms)`));
    }, timeoutMs);
    pending.set(id, {
      resolve: (v) => {
        clearTimeout(t);
        resolve(v);
      },
      reject: (e) => {
        clearTimeout(t);
        reject(e);
      },
    });
    ws.send(JSON.stringify({ type: "req", id, method, params: params ?? {} }));
  });
}

const ws = new WebSocket(wsUrl);

ws.onmessage = (ev) => {
  if (typeof ev.data !== "string") return;
  let data;
  try {
    data = JSON.parse(ev.data);
  } catch {
    return;
  }
  const id = data.id;
  if (!id || !pending.has(id)) return;
  const { resolve, reject } = pending.get(id);
  pending.delete(id);
  if (data.error) {
    reject(new Error(data.error.message || JSON.stringify(data.error)));
    return;
  }
  if (data.ok === false) {
    const p = data.payload;
    const msg =
      p && typeof p === "object" && "error" in p ? String(p.error) : "request failed";
    reject(new Error(msg));
    return;
  }
  resolve(data.payload ?? data.result);
};

ws.onerror = () => {
  /* handled in onclose */
};

ws.onclose = (ev) => {
  for (const [, { reject }] of pending) {
    reject(new Error(`WebSocket closed (code ${ev.code}) before response`));
  }
  pending.clear();
};

const connectMs = Number(process.env.E2E_GATEWAY_CONNECT_MS || 15000);
await Promise.race([
  new Promise((resolve, reject) => {
    ws.onopen = resolve;
    ws.onerror = () => reject(new Error("WebSocket connection failed"));
  }),
  new Promise((_, reject) =>
    setTimeout(() => reject(new Error(`WebSocket connect timeout (${connectMs}ms)`)), connectMs),
  ),
]);

try {
  await sendRpc(ws, "connect", { client: "e2e-voice-stt", version: "0.0.1" }, 30_000);
  const out = await sendRpc(
    ws,
    "voice.transcribe",
    { audio: b64, mimeType: "audio/wav" },
    180_000,
  );
  console.log(JSON.stringify(out));
} catch (e) {
  console.error(String(e && e.message ? e.message : e));
  process.exit(1);
} finally {
  try {
    ws.close();
  } catch {
    /* ignore */
  }
}
