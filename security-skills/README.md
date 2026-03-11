# Security Engineer Skills Collection

A comprehensive, enterprise-grade skills collection for Security Engineers in PDLC (Product Development Lifecycle) environments. **64 skills across 8 plugins with 27 commands**, grounded in real frameworks (NIST, OWASP, ISO 27001, CIS, MITRE ATT&CK).

## Quick Start

**New to this collection?** Start with one of these workflows:

1. **Threat Assessment**: Run `/security-skills/threat-modeling/commands/model-threats`
2. **Application Security Review**: Run `/security-skills/secure-development/commands/review-security`
3. **Cloud Audit**: Run `/security-skills/infrastructure-security/commands/audit-cloud`
4. **Compliance Check**: Run `/security-skills/compliance-governance/commands/assess-compliance`

See [INDEX.md](INDEX.md) for complete navigation and workflow examples.

## The 8 Plugins

| Plugin | Focus | Skills | Commands |
|--------|-------|--------|----------|
| **[Threat Modeling](threat-modeling/)** | Identify & prioritize threats | 9 | 4 |
| **[Secure Development](secure-development/)** | Secure coding & OWASP | 9 | 3 |
| **[Application Security](application-security/)** | Testing (SAST/DAST) | 8 | 3 |
| **[Infrastructure Security](infrastructure-security/)** | Cloud, containers, IAM | 8 | 3 |
| **[Compliance & Governance](compliance-governance/)** | Regulatory frameworks | 8 | 3 |
| **[Incident Response](incident-response/)** | IR planning & response | 8 | 3 |
| **[Security Operations](security-operations/)** | Monitoring & detection | 7 | 3 |
| **[Security Toolkit](security-toolkit/)** | Culture & training | 7 | 3 |

**Total: 64 skills, 27 commands**

## Key Features

Every skill includes:
- ✅ **Domain Context** grounded in real frameworks (NIST, OWASP, ISO, CIS, MITRE)
- ✅ **Step-by-step Instructions** with actionable methodology
- ✅ **Anti-Patterns Section** identifying common security mistakes (especially for LLMs)
- ✅ **Further Reading** citations to authoritative sources
- ✅ **Real-world Applicability** with concrete deliverables

Every command chains 2-4 skills and produces tangible outputs (reports, policies, test plans, etc.)

## What's Inside

```
security-skills/
├── INDEX.md                    # Master guide & workflow examples
├── COMPLETION_SUMMARY.md       # Implementation status
├── threat-modeling/            # STRIDE, attack trees, DFD, risk scoring
├── secure-development/         # Secure coding, OWASP, auth, crypto
├── application-security/       # SAST, DAST, dependency scanning
├── infrastructure-security/    # Cloud, networking, containers, K8s
├── compliance-governance/      # GDPR, SOC2, PCI-DSS, ISO 27001
├── incident-response/          # IR planning, forensics, postmortems
├── security-operations/        # SIEM, detection, monitoring
└── security-toolkit/           # Champion programs, training, metrics
```

Each plugin contains:
- `.claude-plugin/plugin.json` — Plugin configuration
- `README.md` — Plugin overview with all skills and commands
- `skills/` — Detailed skill guides (40-80+ lines each)
- `commands/` — Multi-skill command chains

## Usage Patterns

### Pattern 1: New Security Program
```
1. threat-modeling/model-threats
2. secure-development/review-security
3. infrastructure-security/audit-cloud
4. compliance-governance/assess-compliance
5. security-operations/setup-monitoring
```

### Pattern 2: Breach Response
```
1. incident-response/respond-to-incident
2. incident-response/investigate-breach
3. incident-response/write-postmortem
4. security-toolkit/run-tabletop
```

### Pattern 3: Quarterly Security Review
```
1. application-security/test-security
2. application-security/scan-dependencies
3. threat-modeling/assess-risk
4. compliance-governance/prepare-audit
5. security-operations/design-detections
```

## Skill Examples

### Threat Modeling Skills
- **STRIDE Analysis**: Apply Microsoft's STRIDE framework to identify threats
- **Attack Tree Modeling**: Build hierarchical attack trees with effort/cost analysis
- **Data Flow Diagrams**: Create security-focused DFDs with trust boundaries
- **Risk Scoring**: Quantify threats using likelihood × impact methodology

### Secure Development Skills
- **Secure Coding Review**: Audit code against OWASP guidelines
- **Input Validation**: Implement whitelisting-based validation patterns
- **Output Encoding**: Apply context-specific encoding (HTML, URL, JavaScript, CSS)
- **Authentication Design**: Implement MFA, strong passwords, secure password reset
- **Cryptography Selection**: Choose algorithms and parameters (AES-256-GCM, Argon2, etc.)

### Infrastructure Security Skills
- **Cloud Security Posture**: Assess using cloud provider security tools
- **Network Segmentation**: Design VPCs, security groups, network policies
- **Container Security**: Audit images, runtime policies, seccomp/AppArmor
- **IAM Policy Design**: Implement least-privilege role-based access

### Compliance Skills
- **Compliance Mapping**: Map controls to frameworks (GDPR, SOC 2, PCI-DSS, ISO 27001)
- **Audit Preparation**: Evidence gathering and readiness assessment
- **Data Classification**: Classify data and define protection requirements

## Who This Is For

- **Security Engineers** building/improving security programs
- **CISO/Security Leaders** planning initiatives and compliance
- **Security Architects** designing threat models and controls
- **Security Teams** conducting assessments and audits
- **DevOps/Platform Engineers** implementing infrastructure security
- **Developers** conducting security code reviews

## Framework References

All content is grounded in industry standards:
- **NIST** Cybersecurity Framework, SP 800-30, SP 800-53, SP 800-57
- **OWASP** Top 10, OWASP Testing Guide, CWE Top 25
- **ISO/IEC** 27001, 27005
- **CIS** Controls
- **MITRE ATT&CK** Framework
- **Microsoft STRIDE** Threat Modeling
- **SOC 2**, **PCI-DSS**, **GDPR**, **HIPAA**

## Files & Structure

- **18 fully-implemented deep-dive skills** (threat-modeling + secure-development plugins)
- **27 command files** with multi-skill chains
- **8 plugin.json** configurations
- **8 README.md** files with skill/command listings
- **64 skill descriptions** across all plugins
- **Master INDEX.md** for navigation and workflow examples
- **COMPLETION_SUMMARY.md** with implementation details

## Next Steps

1. **Read**: Start with [INDEX.md](INDEX.md) for overview
2. **Pick a Command**: Choose one that matches your need
3. **Follow the Chain**: Execute the skills in sequence
4. **Collect Output**: Each skill produces concrete deliverables
5. **Iterate**: Commands suggest follow-ups for continuous improvement

## Design Philosophy

**Comprehensive**: Covers full security lifecycle from threat identification → development → testing → operations → incidents → compliance

**Grounded**: Every skill references real frameworks; no generic advice

**Actionable**: Step-by-step methodologies with concrete deliverables (threat models, security policies, test plans, reports)

**Error-Resistant**: Anti-patterns section in every skill identifies common mistakes LLMs make in security work

**Self-Contained**: Each skill is independent but commands chain them together

## License

MIT License. Free to use, modify, and distribute.

---

**Version**: 1.0.0  
**Created**: 2026-03-11  
**Author**: Seth Ford  
**Status**: Production-ready collection with extensible architecture

For detailed implementation status and next steps, see [COMPLETION_SUMMARY.md](COMPLETION_SUMMARY.md).
