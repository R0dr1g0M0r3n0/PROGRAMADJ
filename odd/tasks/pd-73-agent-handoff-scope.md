# PD-73 Agent Handoff and Scope Policies

## Objective

Define a repository-local agent handoff contract that prevents unsafe acceptance, silent acceptance-test changes, scope expansion, and access to original audio or DJ library files.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; Jira status update only after verified completion.
- Do not touch or stage `.atl/` or original audio/DJ library files.
- Bounded slice: three files maximum, including this task document.
- Strict TDD: RED → GREEN → REFACTOR.

## Work units

- [x] **T1 — Define the handoff policy.** Added the policy and acceptance rules.
- [x] **T2 — Add structural contract checks.** Added focused checks for required controls and protected paths.
- [x] **T3 — Verify and record closure evidence.** Focused and full checks passed; closure evidence is recorded below.

## Route

- Route: direct inline, bounded to three files because the policy is a self-contained documentation contract with one structural test.
- Trigger evidence: existing PD-71/PD-72 harness patterns and no pre-existing agent-policy implementation.

## Acceptance criteria

- Unsafe or missing proof blocks acceptance.
- Agents cannot silently change acceptance tests or authorized scope.
- Handoffs record inputs, outputs, authorized files, acceptance criteria, limits, and independent verification.
- Original audio and DJ library paths are explicitly protected.
- Exact checks, outputs, failures, and skipped checks are recorded.

## Checks

- `python3 -m unittest Tests/Harness/test_agent_handoff_policy.py`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `git diff --check`

## Verification evidence

- RED: `python3 -m unittest Tests/Harness/test_agent_handoff_policy.py` failed because `docs/agent-handoff-policy.md` did not exist.
- GREEN: focused policy checks passed **2/2**.
- Full Python harness passed **15/15**.
- `git diff --check`: PASS.
- Independent verification: delegated verifier reproduced all three checks with no failures or skips.
- `.atl/` and original audio/DJ library paths were untouched.
- Work-unit commit: `e1c6cf3` (`feat(contracts): define agent handoff scope policy`).
