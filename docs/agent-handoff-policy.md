# Agent Handoff and Scope Policy

## Purpose

Every delegated AudioForge task must carry enough information for an independent reviewer to reproduce the result without trusting an unsupported completion claim. This policy applies to implementation, verification, and documentation agents.

## Required handoff

A handoff is valid only when it names all of the following:

- **Authorized files**: the exact repository-relative files and directories the agent may read or change.
- **Inputs**: issue key, prerequisite evidence, fixtures, configuration, and immutable baseline references.
- **Outputs**: expected files, artifacts, command results, and bounded side effects.
- **Acceptance criteria**: observable conditions that determine pass or failure; acceptance tests are not agent-editable evidence.
- **Resource limits**: maximum files, commands, runtime, and any protected external resources.
- **Independent verification**: a check performed outside the implementation step, with the command and result recorded.

An agent may report a limitation or request a scope decision, but may not silently expand authorized files, rewrite acceptance criteria, or claim completion without the required evidence.

## Acceptance gate

Unsafe or missing proof **must be rejected**. Acceptance requires reproducible evidence for every criterion: exact checks, outputs, failures, and skipped checks. A failed, unavailable, timed-out, or skipped check remains visible and cannot be converted into success by wording alone.

The implementing agent cannot silently change acceptance tests. Any change to an acceptance test, authorized file, input contract, or resource limit is a scope change and requires an explicit new handoff plus independent verification.

## Protected paths

Original audio files, DJ library files, Rekordbox databases, exports, credentials, and personal metadata are outside the authorized repository scope. Agents must not copy, modify, stage, or use them as test fixtures. Synthetic fixtures and repository-local evidence must remain free of personal media paths.

## Evidence record

The closure record must include:

1. Handoff scope and the files actually changed.
2. Inputs and outputs, including exact commands and exit codes.
3. Acceptance criteria and the evidence for each one.
4. Failed, unavailable, timed-out, and skipped checks, if any.
5. Independent verification command, result, and reviewer boundary.
6. Any limitation, unresolved risk, or requested follow-up.

Only after this record is complete may the task be marked accepted.
