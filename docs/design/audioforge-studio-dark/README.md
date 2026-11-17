# AudioForge Studio — Titanium Dark

**v4-dark · generated-concepts-not-implementation.** The NEW Titanium direction and workflow/structure are approved, not every generated pixel. This passive catalog changes no code, audio, or source media; it makes no functional, calibration, or accessibility-compliance claim.

## Full catalog

Six modalities and ten supporting examples across eight planned PNGs. Links open files only; no thumbnails or embedded previews.

| File | Modality / supporting examples |
| --- | --- |
| [01-library.png](01-library.png) | Library |
| [02-dj-prep.png](02-dj-prep.png) | DJ Prep |
| [03-warp.png](03-warp.png) | Warp |
| [04-restore.png](04-restore.png) | Restore |
| [05-draw.png](05-draw.png) | Draw |
| [06-analyze.png](06-analyze.png) | Analyze |
| [07-processing-windows.png](07-processing-windows.png) | Spectral De-noise; Deconstruct; Music Rebalance; Module Chain (4) |
| [08-workflow-windows.png](08-workflow-windows.png) | Compare & History; Batch Queue; Export; Audio & Shortcuts; Project & Files/Relink; Presets & Module Details (6) |

[manifest.json](manifest.json) records the catalog and design semantics. [prompts.json](prompts.json) is a planned parent-supplied file, not created by this documentation work.

Asset presence at documentation creation: 1/8 PNGs present, 7/8 pending parent supply. Present: `03-warp.png`. `prompts.json`: pending-parent-supplied. Presence is not visual review or acceptance; pending links may not open until supplied. The parent may supply assets independently after this snapshot.

## Shared design constraints

- **Dark everywhere:** black/near-black backgrounds for the titlebar, controls, inspector, transport, menus, dialogs, and every supporting window. Silver text and deep-petrol active UI; no white or bright panels. This preserves the user's comfort constraint, not a verified accessibility outcome.
- **One shared studio:** six-mode segmented belt, prominent numeric readings, top full-song overview, central stereo canvas, contextual instrument-rack inspector, and bottom transport. Supporting windows are examples, not additional modes.
- **Retain full RX scope:** the illustrated processing and workflow windows are examples only, not an exhaustive module inventory or a reduction of the existing RX feature scope.
- **Honest parameters:** module algorithms, parameter ranges/defaults, processing results, and calibration are unresolved here. Any depicted values are illustrative, not validated specifications.

## Waveform and color semantics

| Layer | Concept intent |
| --- | --- |
| 3Band overview | Blue lows, amber mids, pale/near-white highs, confined to signal marks on dark surfaces; not bright backgrounds. |
| Transients | Clear attacks, not decorative smoothing or uniform synthetic peaks. |
| Metadata overlays | Cue points, phrase regions, beat grid, and warp anchors remain independent, distinguishable layers; frequency color must not replace them. |
| Draw | True PCM sample stems at sample scale. Edits represent per-sample amplitude changes, never fabricated frequency-band edits. Only the top overview is multiband in Draw. |
| Source preservation | Original audio is unchanged; all processing and edit depictions are conceptual. |

The [official rekordbox 3Band announcement](https://rekordbox.com/en/2020/04/new-rekordbox-dj-application/) establishes blue lows, amber mids, and white highs. AudioForge adapts highs to pale signal marks for this dark concept. The [rekordbox 7.2.8 manual](https://cdn.rekordbox.com/files/20251202174516/rekordbox7.2.8_manual_EN.pdf) lists BLUE, RGB, and 3Band waveform schemes (pp. 89, 228). These references do not establish AudioForge frequency cutoff values: cutoffs remain undecided.

## Documentation checks

- JSON syntax: `python3 -m json.tool docs/design/audioforge-studio-dark/manifest.json >/dev/null`.
- Structural readback: eight unique PNG filenames, six modalities, ten supporting examples, and ordinary file links without image embeds.
- No build, functional tests, image generation, source edits, Jira operations, or Git operations are part of this documentation work. Asset pixels, audio behavior, calibration, and accessibility compliance are not verified.
