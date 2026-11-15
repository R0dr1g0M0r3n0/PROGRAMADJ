# Project manifest v1: non-destructive contract

`project-manifest-v1.schema.json` defines the shape of an AudioForge project. The
manifest records references and decisions; it is not an audio container, a DJ
library export, or a save/load implementation. `TestFixtures/project_manifest_v1.json`
is synthetic design evidence, not a production file.

## Quick path

1. Read `schemaVersion` before interpreting any other field. Version 1 is the
   only version defined here.
2. Validate the manifest against the version-matched JSON Schema, then apply
   the semantic invariants below. Schema validation alone is insufficient.
3. Resolve source paths read-only and compare the recorded SHA-256 digest before
   using their contents. A missing or changed source is an unresolved reference,
   not permission to overwrite it.

## Ownership and references

| Record | Owns | References |
| --- | --- | --- |
| `sourceAssets` | Import identity, exact original path, original digest, import time | External files, read-only |
| `derivedAssets` | Project-generated path and digest, immutable lineage | Root source, immediate parent asset, processing profile |
| `tracks` | Project-owned metadata and selected active asset | Original audio source and an asset in its lineage |
| `regions`, `cues`, `loops`, `beatgrids`, `phrases` | Project annotations in a specific asset's frame domain | Track and source/derived audio asset in that track's lineage |
| `history` | Append-only record of project decisions | Track, typed target, input/output assets, optional profile |
| `profiles` | Snapshot of processing/analysis parameters | Referenced by derived assets or history |
| `validityClaims` | Append-only assertion and evidence provenance | Typed subject, evidence assets, optional profile/history |

All IDs are lowercase UUID strings, globally unique across every record array
and the project ID. An ID identifies a logical record, never a path or array
position. IDs are not recycled, even after a record is no longer active. All
references must resolve to an existing record of the documented kind. Deletion
must not leave dangling references; future implementations may retain inactive
records rather than erase referenced history.

### Asset lineage and original preservation

- A source asset's `originalPath` is the exact path captured at import; do not
  normalize, rewrite, relocate, or substitute it during migration. Its SHA-256
  and optional byte size describe the imported bytes. A changed file requires
  explicit relinking or re-import under a new identity; never silently accept
  the new bytes under the old digest.
- A derived asset's `relativePath` is project-owned and must be a normalized,
  non-empty relative path inside a designated derived-asset directory. It must
  not be absolute, contain `..` segments, or resolve to a source path (including
  through symlinks). The schema checks only non-empty text; safe path resolution
  is a PD-68 responsibility.
- `parentAssetId` resolves to either a source or a derived asset. Following
  parent links must be acyclic and end at `rootSourceAssetId`, which resolves to
  a source audio asset. The derived asset's root must equal its parent's root.
  A generated output must never overwrite any source or prior derived asset;
  replacement creates a new asset ID and digest. Its `profileId` resolves to a
  processing profile snapshot.
- A track's `sourceAssetId` resolves to an audio source. Its `activeAssetId`
  resolves to that source or a derived descendant with the same root. Switching
  the active asset does not remove prior assets, annotations, or history.
- External DJ library files may be referenced as `sourceAssets` with kind
  `dj_library`, but this contract neither parses nor exports them. The manifest's
  `userMetadata` is project-owned JSON, not a promise to round-trip or modify
  external library metadata.

### Time, annotations, and provenance

- Every timestamp uses UTC `YYYY-MM-DDTHH:mm:ss.sssZ`; `updatedAt` cannot
  precede `createdAt`. A writer must produce calendar-valid timestamps with
  deterministic millisecond precision. An import's `importedAt`, profile or
  derived asset's `createdAt`, history `at`, and claim `assertedAt` are event
  timestamps, not local-time display values.
- All frame indices are non-negative integers in the referenced `assetId`'s
  frame domain. Spans are half-open `[startFrame, endFrame)` with `startFrame <
  endFrame`. Cues point to one frame. Annotations must reference an audio asset
  in their track's lineage; coordinates are never silently transferred to a
  different derived asset. Frame bounds require asset media metadata, which is
  outside this version's manifest contract.
- Beatgrid anchors have strictly increasing `frame` and `beatIndex` values in
  array order. At least two anchors are required. No BPM, musical correctness,
  or confidence is inferred from the mere presence of anchors.
- History entries are append-only. `inputAssetIds` resolve to the track's
  lineage; optional `outputAssetId` is a derived asset in that lineage.
  `profileId`, when present, resolves to a profile snapshot. An entry records
  intent/evidence but does not by itself prove that an operation executed.
- Validity is never inferred from record presence. A claim's `subject.kind`
  must match its target array. `provenance.assetIds` and optional `profileId`
  and `historyId` must resolve. Import-origin claims must include the imported
  source asset in `assetIds`; analysis-origin claims must identify a profile or
  history entry. Claims for a subject form a linear, acyclic chain through
  `supersedesClaimId` (oldest claim omits it). The terminal claim is current;
  no claim means `unknown`. Conflicting terminal claims are invalid, not a
  tie to resolve by timestamp. Older claims remain intact for audit.

## Compatibility and migration policy

`schemaVersion` is a required integer contract version, not the Engine C ABI
version or the unrelated synthetic DJ collection fixture's `schema_version`.
The v1 schema rejects unknown core fields; later versions receive a separate
schema file. A reader must select by exact version, reject unsupported future
versions without rewriting them, and preserve the original project bytes on
failure. Do not guess a downgrade or discard fields.

PD-68 should implement explicit, version-to-version, pure migrations over a
copy of the manifest. Each step validates input and output shape **and** the
semantic invariants; it preserves stable IDs, original paths/digests, history,
metadata, and validity provenance. Before replacing a project manifest,
retain a recoverable backup and write the new version atomically. If validation
or replacement fails, leave the prior version usable. Migration tests must
cover old-to-new, already-current (idempotent), malformed, unknown-future, and
interrupted-write cases. This document defines policy only; no migration or
persistence code is part of PD-67.

## Verification boundary

JSON Schema checks field types, required fields, enums, ID/digest/timestamp
lexical shapes, and local array constraints. It does **not** check global ID
uniqueness, reference resolution, path containment, ancestry, chronological
ordering, calendar validity, frame ordering/bounds, or claim chains. Those
semantic checks and actual file I/O belong to PD-68. The fixture illustrates
the intended relationships but is not proof of a live round trip, audio
processing, or DJ library compatibility.
