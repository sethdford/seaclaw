# Security Engineer Skills Collection

Comprehensive security skills collection for the Security Engineer role in PDLC environments. **64 skills across 8 plugins with 27 commands.**

## Plugin Overview

| Plugin | Skills | Commands | Focus |
|--------|--------|----------|-------|
| [Threat Modeling](threat-modeling/) | 9 | 4 | STRIDE, attack trees, DFDs, risk scoring |
| [Secure Development](secure-development/) | 9 | 3 | Secure coding, OWASP, auth, crypto |
| [Application Security](application-security/) | 8 | 3 | SAST, DAST, dependency scanning, APIs |
| [Infrastructure Security](infrastructure-security/) | 8 | 3 | Cloud, networking, containers, K8s, IAM |
| [Compliance & Governance](compliance-governance/) | 8 | 3 | Regulatory frameworks, policies, audits |
| [Incident Response](incident-response/) | 8 | 3 | IR planning, forensics, postmortems |
| [Security Operations](security-operations/) | 7 | 3 | SIEM, detection, monitoring, vulnerabilities |
| [Security Toolkit](security-toolkit/) | 7 | 3 | Culture, training, metrics, red teams |

**Total: 64 skills, 27 commands**

## Plugin Details

### 1. Threat Modeling (threat-modeling/)
Master threat identification and risk assessment using industry frameworks.

**Skills:**
- stride-analysis — Apply STRIDE threat framework
- attack-tree-modeling — Build hierarchical attack trees
- data-flow-diagram-security — Create security-focused DFDs
- threat-identification — Identify threats from libraries and MITRE ATT&CK
- abuse-case-design — Design negative use cases
- trust-boundary-analysis — Identify privilege transitions
- asset-inventory — Enumerate critical assets
- threat-library — Build reusable threat library
- risk-scoring — Quantify likelihood and impact

**Commands:**
- `model-threats` — Comprehensive threat modeling
- `analyze-attack-surface` — Identify exposed entry points
- `assess-risk` — Quantify organizational risk
- `map-trust-boundaries` — Document trust zones

### 2. Secure Development (secure-development/)
Design and implement secure applications using OWASP and industry best practices.

**Skills:**
- secure-coding-review — Review code for vulnerabilities
- owasp-top-ten-check — Audit against OWASP Top 10
- input-validation-patterns — Implement secure input validation
- output-encoding — Encode output by context
- authentication-design — Design secure authentication
- authorization-design — Design least-privilege access
- session-management — Implement secure sessions
- cryptography-selection — Choose appropriate algorithms
- secrets-management — Manage credentials securely

**Commands:**
- `review-security` — Full security code review
- `audit-auth` — Audit authentication/authorization
- `check-owasp` — OWASP Top 10 assessment

### 3. Application Security (application-security/)
Test applications for vulnerabilities using SAST, DAST, and security scanning.

**Skills:**
- sast-configuration — Configure static analysis tools
- dast-test-plan — Design dynamic testing plans
- dependency-vulnerability-scan — Scan for known CVEs
- security-test-plan — Plan security testing
- penetration-test-scope — Define pen test scope
- api-security-review — Audit API security
- web-security-headers — Implement security headers
- content-security-policy — Design CSP policies

**Commands:**
- `scan-dependencies` — Vulnerability and SBOM scan
- `test-security` — Execute comprehensive security testing
- `review-api-security` — API security assessment

### 4. Infrastructure Security (infrastructure-security/)
Secure cloud, networks, containers, and systems from infrastructure perspective.

**Skills:**
- cloud-security-posture — Assess cloud security
- network-segmentation — Design network isolation
- container-security-review — Audit containers and images
- kubernetes-security — Harden K8s clusters
- iam-policy-design — Design least-privilege IAM
- infrastructure-hardening — Harden servers and systems
- certificate-management — Manage TLS certificates
- zero-trust-architecture — Design zero-trust models

**Commands:**
- `audit-cloud` — Cloud security posture assessment
- `harden-infrastructure` — Implement hardening
- `review-iam` — IAM policy audit

### 5. Compliance & Governance (compliance-governance/)
Achieve and maintain regulatory compliance across frameworks.

**Skills:**
- compliance-mapping — Map controls to frameworks
- security-policy-template — Create security policies
- audit-preparation — Prepare for audits
- gdpr-assessment — Assess GDPR compliance
- soc2-controls — Implement SOC 2 controls
- pci-dss-review — Review PCI-DSS compliance
- data-classification — Classify organizational data
- privacy-impact-assessment — Conduct PIAs

**Commands:**
- `assess-compliance` — Compliance gap analysis
- `prepare-audit` — Audit readiness preparation
- `classify-data` — Data classification framework

### 6. Incident Response (incident-response/)
Plan and execute incident response from detection to recovery.

**Skills:**
- incident-response-plan — Develop IR playbooks
- forensic-analysis-guide — Conduct forensics
- incident-communication — Manage incident communication
- containment-strategy — Develop containment tactics
- evidence-preservation — Preserve evidence chain of custody
- root-cause-analysis-security — Analyze root causes
- recovery-procedures — Execute recovery
- lessons-learned — Conduct post-incident reviews

**Commands:**
- `respond-to-incident` — Execute IR procedures
- `investigate-breach` — Conduct forensic investigation
- `write-postmortem` — Document post-incident analysis

### 7. Security Operations (security-operations/)
Build and operate security monitoring and detection capabilities.

**Skills:**
- siem-rule-design — Design SIEM detection rules
- detection-engineering — Build detection logic
- vulnerability-management-program — Establish vuln management
- security-monitoring-strategy — Design SOC
- log-analysis-security — Analyze logs forensically
- alert-triage — Triage security alerts
- threat-intelligence-integration — Integrate threat intel

**Commands:**
- `design-detections` — Create SIEM rules
- `manage-vulnerabilities` — Establish vuln program
- `setup-monitoring` — Deploy monitoring infrastructure

### 8. Security Toolkit (security-toolkit/)
Build security culture, training, and organizational capabilities.

**Skills:**
- security-champion-program — Establish champion programs
- security-awareness-training — Design awareness training
- secure-architecture-review — Review designs
- bug-bounty-program — Manage bug bounty
- security-metrics-dashboard — Build metrics dashboards
- red-team-exercise — Plan red team exercises
- security-documentation — Document security processes

**Commands:**
- `launch-champion-program` — Establish champion program
- `design-training` — Implement awareness training
- `run-tabletop` — Execute tabletop exercises

## Using These Skills

### Workflow Examples

**New Security Program:**
1. Use threat-modeling `model-threats` to understand risks
2. Use secure-development `review-security` to fix code
3. Use infrastructure-security `audit-cloud` to harden infrastructure
4. Use compliance-governance `assess-compliance` for regulatory readiness
5. Use security-operations `setup-monitoring` to detect threats

**Breach Scenario:**
1. Use incident-response `respond-to-incident` to execute IR
2. Use incident-response `investigate-breach` for forensics
3. Use incident-response `write-postmortem` for lessons learned
4. Use security-toolkit `run-tabletop` to prepare team

**Quarterly Security Review:**
1. Use application-security `test-security` for AppSec posture
2. Use application-security `scan-dependencies` for vulnerability management
3. Use compliance-governance `prepare-audit` for audit readiness
4. Use security-operations `design-detections` to improve detection

## Quick Access

- **Threat Analysis**: threat-modeling plugin
- **Code Security**: secure-development plugin
- **Testing**: application-security plugin
- **Infrastructure**: infrastructure-security plugin
- **Regulations**: compliance-governance plugin
- **Incidents**: incident-response plugin
- **Monitoring**: security-operations plugin
- **Culture/People**: security-toolkit plugin

## Key Features

All skills include:
- ✅ **Domain context** grounded in real frameworks (NIST, ISO 27001, OWASP, CIS, MITRE ATT&CK)
- ✅ **Step-by-step instructions** with actionable methodology
- ✅ **Anti-patterns section** identifying common LLM mistakes in security work
- ✅ **Further reading** citations to authoritative sources
- ✅ **Real-world applicability** with concrete deliverables

All commands:
- ✅ **Multi-step skill chains** connecting related security disciplines
- ✅ **Concrete deliverables** (reports, policies, test plans, etc.)
- ✅ **Follow-up suggestions** for continuous improvement

---

Created: 2026-03-11 | Author: Seth Ford | License: MIT
