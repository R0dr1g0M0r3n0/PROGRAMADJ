# Task State Machine

> **PD-69 architectural decision.** This is a proposed contract for task lifecycle and durable task records, not an inherited runtime or project-manifest contract. PD-69 defines it; PD-70 is responsible for implementation and recovery/retry-limit proof.

Task state is derived from durable, committed transition records for that task. Project manifest history remains separate: it may describe project-level audit intent or evidence, but it is not authoritative for task state or execution success.

## States and transitions

| Current state | Event and guard | Next state |
|---|---|---|
| — | Task is created | pending |
| pending | Start only after every prerequisite is succeeded; bind the attempt to its exact input commit | running (initial attempt 0) |
| running | Execution completes successfully and exact execution evidence is available | succeeded |
| running | Execution fails and failure evidence is available | failed |
| failed | A correction attempt remains; record its attempt number and exact input commit before execution | running |
| failed | Initial attempt plus all three permitted correction attempts have failed | blocked |

No other transition is valid. In particular, succeeded and blocked are terminal. A task with unmet prerequisites stays pending; it must not run. A failed attempt is not itself blocked while correction attempts remain.

## Invariants

- **Attempt numbering:** the initial execution is attempt 0. At most three correction attempts (1 through 3) may follow it. A retry consumes its correction attempt when it transitions to running; it cannot be reused.
- **Input binding:** every running attempt is bound to the exact input commit used for that execution. Any change of input commit between attempts must be captured by that attempt's committed transition record.
- **Evidence:** a transition to succeeded requires exact evidence identifying the execution and its successful result. A mere start intent, process launch, or missing result is not success. A failed attempt likewise requires evidence of its failure.
- **Record authority:** committed, append-only task transition records are the sole authority for task state. A proposed or partially written record is intent only and must not advance state. Existing records are never rewritten or deleted to repair history.
- **Replay:** state is reconstructed by replaying committed records in order and validating their versions, sequence, and allowed transitions. Invalid, corrupt, or unsupported-version records fail closed: do not guess, skip, or infer a state.
- **Interruption:** a crash or interruption without a committed terminal result cannot infer succeeded. Recovery behavior is not specified here; PD-70 must prove it.
- **Separation:** project-manifest history and task transition records have distinct responsibilities. Manifest entries cannot substitute for task transition records or their execution evidence.

## Version 1 transition-record shape

`docs/task-transition-record-v1.schema.json` describes one record. Each version 1 record carries `schemaVersion` (exactly `1`), `taskId`, a per-task `sequence` starting at `1` and increasing by one, `fromState`, `toState`, `event`, `attempt`, and `prerequisiteIds`. The prerequisite identifiers are repeated on each record; all prerequisites must have succeeded before the first attempt starts.

| Event | Required transition data |
|---|---|
| `created` | `null` → `pending`, attempt `0` |
| `attempt_started` | `pending` → `running` at attempt `0`, or `failed` → `running` at correction attempt `1`–`3`; include the exact `inputCommit` |
| `attempt_succeeded` | `running` → `succeeded`; include `executionEvidence` with the exact command argument vector and a successful result |
| `attempt_failed` | `running` → `failed`; include `executionEvidence` with the exact command argument vector and a failed result |
| `attempts_exhausted` | `failed` → `blocked` at attempt `3`, after the initial attempt and all three corrections failed |

Execution evidence has a `command` argument array and a `result` containing `outcome` (`success` or `failure`) and non-empty `detail`. The event, target state, and result outcome must agree. The version 1 schema does not validate ordered history by itself; replay must enforce per-task sequence, legal transitions, prerequisite completion, attempt order, and terminal-state rules. An unknown schema version remains fail-closed.

## Durability and implementation boundary

The implementation must make the distinction between an uncommitted intent and a committed durable transition observable, preserve append-only history, and replay deterministically with fail-closed validation. This contract intentionally does not prescribe storage encoding, filesystem/database strategy, or a specific commit protocol. PD-70 owns those implementation details and the tests for restart recovery, corruption/unknown-version handling, and the correction-attempt limit.

**Status:** contract only. No runtime implementation or recovery behavior is claimed by this document.
