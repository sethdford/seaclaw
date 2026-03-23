# ESCALATE.md — human Approval Protocol

Version: 1.0.0
Standard: ESCALATE.md v1.0

This file defines the structured approval protocol for the human AI assistant runtime.
Actions not listed here default to **approve** (require explicit approval).

## Approval Matrix

| Action | Level | Timeout | Channel |
| --- | --- | --- | --- |
| file_read | auto | 0 | |
| file_list | auto | 0 | |
| web_search | auto | 0 | |
| web_fetch | auto | 0 | |
| memory_recall | auto | 0 | |
| memory_store | auto | 0 | |
| calculator | auto | 0 | |
| code_analyze | auto | 0 | |
| file_write | notify | 0 | |
| file_edit | notify | 0 | |
| git_status | auto | 0 | |
| git_diff | auto | 0 | |
| git_log | auto | 0 | |
| git_commit | notify | 0 | |
| git_push | approve | 120 | |
| shell_* | approve | 300 | |
| deploy_* | approve | 600 | |
| database_write | approve | 300 | |
| database_delete | approve | 300 | |
| send_email | approve | 120 | |
| send_message | approve | 120 | |
| api_call_* | approve | 300 | |
| secret_* | deny | 0 | |
| config_security | deny | 0 | |
| system_admin | deny | 0 | |

## Level Definitions

- **auto**: Execute immediately without user interaction.
- **notify**: Execute immediately; inform the user after completion.
- **approve**: Pause execution; require explicit user approval before proceeding. Timeout in seconds (0 = no timeout, wait indefinitely).
- **deny**: Never execute. Return an error explaining the action is not permitted.

## Compliance Notes

- This protocol aligns with EU AI Act human oversight requirements (Article 14).
- All escalation decisions are logged to the audit trail with HMAC chain integrity.
- Approval timeouts cause the action to be denied (fail-safe).
