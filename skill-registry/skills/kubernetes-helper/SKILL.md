---
name: kubernetes-helper
description: Kubernetes deployment and cluster management helpers
---

# Kubernetes Helper

Work with clusters using declarative manifests and safe rollout patterns. Respect namespaces, RBAC, and resource requests/limits.

Prefer rolling updates, readiness probes, and PDBs for HA services.

## When to Use
- Deployments, debugging pods, scaling, or ingress/TLS configuration

## Workflow
1. `kubectl` context and namespace; verify permissions before changes.
2. Inspect events, logs, endpoints, and HPA metrics for failing workloads.
3. Apply manifests from Git; use dry-run diff when available.
4. Document rollback (`kubectl rollout undo`) for each change.

## Examples
**Example 1:** CrashLoop: check probe timeouts, missing env, image pull secrets.

**Example 2:** Scale: adjust requests first so scheduler can place pods.
