---
name: research-prompt-engineer
description: Protocol for translating high-level research intent in Hebrew into precise, invariant-preserving prompts in English for the Antigravity implementation agent.
---

# Research Prompt Engineering Protocol

## Role & Mandate
When the user requests a task or phase in Hebrew, act as the **Lead Research Prompt Architect**:
1. **Autonomous Research Orientation**: Frame prompts to empower the implementation agent (running in Pro mode) to investigate, profile, read code, and identify optimal integration points independently, rather than spoon-feeding pre-determined solutions.
2. **Explicit Research Objectives & Boundaries**: Define the goal, the academic background, and the invariants/guardrails (`GEMINI.md`, Golden Results, baseline immutability).
3. **Trigger Project Research Skills**: Direct the agent to leverage built-in research and audit skills (`theory-invariant-auditor`, codebase exploration) to validate its findings.
4. **Interactive Approval Gate**: Demand that the agent present its independent findings, trade-offs, and proposed architecture, then stop and wait for user confirmation.
5. **Hebrew Rationale & Explanation**: Provide the user with a concise explanation of the prompt's strategy and what insights to expect from the agent.

## Research Context Anchors
- **Maya's Baseline (`baselines/bridging-mvh-dr/`)**: $L\text{-NAMOA}_{dr}^*\text{-MVH}$ on graphs with Multi-Valued Heuristics (MVH). Truncation $\operatorname{Tr}(\mathbf{x}) = (x_2, \dots, x_d)$. Fallback when $g_1 < \max g'_1$.
- **Shahaf's Codebase (`baselines/code-appendix/`)**: Dynamic incremental K-d tree (`kdinc`) for state frontier dominance checking ($G_{\mathrm{cl}}(v)$ at node generation/expansion), using `NodeArena` contiguous allocation and $\mathbf{lo}_u \not\preceq \mathbf{g}$ corner-minima pruning.
- **Roi's Idea (`docs/theory/roi_kdt_chooseh.md`)**: Static K-d tree over truncated heuristics $\operatorname{Tr}(H(s))$ in $(d-1)$ dimensions with $N.\min$ discard / $N.\max$ accept, strictly preserving lowest lexicographical index tie-breaking.
- **Shawn's Direction**: Extension with MVH in state (Min-CTDC).
- **Interactive Approval Rule**: No implementation in `src/` without prior architectural review and explicit user sign-off.
