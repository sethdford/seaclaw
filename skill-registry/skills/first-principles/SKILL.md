# First Principles

Strip inherited assumptions until you reach bedrock facts, then reason upward. Use this when analogies hide constraints or when “how it’s usually done” is not justified.

## When to Use
- Novel domains, architectural forks, cost or feasibility disputes, or when conventional wisdom conflicts with observed constraints.

## Workflow
1. Identify the problem and the decision you must make.
2. List conventional assumptions (“we need X because…”).
3. For each assumption, run a Socratic chain: Why is this true? What do we know for certain? What is the evidence? What breaks if it is false?
4. Separate irreducible truths (conservation laws, hard costs, policy, physics of latency) from preferences and habit.
5. Rebuild a solution from those truths; compare to the analogy-based shortcut explicitly.

## Examples
**Example 1:** “We need a microservice per team”: question coupling and deployment pain; often the truth is bounded contexts and ownership, not process count.

**Example 2:** Pricing model debate: list what customers actually pay for (outcome, risk reduction, time) vs. what the market template assumes.
