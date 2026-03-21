---
name: backup-scheduler
description: Schedule and manage backup jobs
---

# Backup Scheduler

Ensure recoverability with encrypted, tested backups. Follow 3-2-1 rule where practical; verify restores regularly—backups untested are wishful thinking.

Classify RPO/RTO per dataset; not everything needs the same schedule.

## When to Use
- Database admin, filesystem snapshots, or compliance prep

## Workflow
1. Inventory data stores, sizes, and legal retention requirements.
2. Automate backup jobs; encrypt at rest and in transit; restrict access.
3. Periodic restore drill to cold storage; measure time to recover.
4. Document failure scenarios: region loss, ransomware, operator error.

## Examples
**Example 1:** Nightly DB dump + weekly full to object storage with lifecycle rules.

**Example 2:** Application-aware snapshots before risky migrations.
