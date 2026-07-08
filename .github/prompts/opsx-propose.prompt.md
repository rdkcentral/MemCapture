---
description: Propose a new change - create it and generate all artifacts in one step
license: MIT
attribution:
  source: OpenSpec Fision AI
  note: Adapted for MemCapture repository workflows.
---

Create and initialize a new OpenSpec change with proposal, design, and tasks.

Input:
- Change name (kebab-case), or
- A short feature description from which a change name can be derived.

Flow:
1. Resolve a change name. If unclear, ask the user one open question.
2. Run `openspec new change "<name>"`.
3. Run `openspec status --change "<name>" --json`.
4. For each ready artifact, run `openspec instructions <artifact-id> --change "<name>" --json`.
5. Create artifact files using provided templates and dependency context.
6. Re-check status until required apply artifacts are marked done.
7. Summarize results and suggest `/opsx:apply` for implementation.

Guardrails:
- Keep artifacts repository-specific (MemCapture, embedded Linux/RDK constraints).
- Do not paste instruction metadata into output artifacts.
- Ask before overwriting an existing change directory.
