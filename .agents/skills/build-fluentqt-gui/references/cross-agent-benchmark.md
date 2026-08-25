# Cross-agent benchmark

Use this only when evaluating the Skill itself. It is not part of the normal
GUI build workflow.

## Fixed inputs

- Package the Skill once and give the same archive, without local edits, to
  Codex and Cursor.
- Pin the same FluentQt source revision or installed development package and Qt
  toolchain for both runs. Record the selected prefix or source path and keep a
  `CMakeCache.txt` or equivalent dependency-resolution artifact. Selecting a
  different or Inspector-incapable install makes that machine check `false`;
  a compatibility placeholder is not an Inspector pass.
- Use the prompt in `assets/benchmarks/agent-run-workspace.json` verbatim.
- Give each implementation agent the target repository and prompt only. Do not
  provide an intended layout, a previous result, or a diagnosis of likely
  mistakes.
- Use a different person or agent for the final blind review.

## Initialize one run

Create one manifest per agent before implementation starts:

```bash
python3 <skill-root>/scripts/benchmark_run.py init \
  --agent codex \
  --author-id codex-runner \
  --skill-package /absolute/path/build-fluentqt-gui.zip \
  --workspace /absolute/path/target-repository \
  --output runs/codex/run.json
```

Repeat with `cursor`. Keep artifact and log paths relative to the run manifest
so the run directory can be archived or published intact.

## Record the run

Set `status` to `running` and record `started_at` when work begins. Record:

- every build, test, Inspector, architecture, and visual-evidence command with
  its exit code, duration, and log path;
- the design brief, project-structure manifest, visual-evidence manifest,
  Inspector report, and built application path;
- each machine check as `true` or `false`, never as an inferred score;
- a terminal status of `completed` or `blocked` plus `completed_at`.

For a completed run, the independent reviewer scores all nine dimensions from
1 to 5 and attaches evidence paths. A blocked run records the blocker without
inventing missing artifacts or scores.

Do not copy the implementation agent's embedded visual verdict into the run
manifest. The benchmark reviewer must inspect the final pixels independently,
including a wide state at 1x, and record their own scores after implementation
ends. A valid visual-evidence manifest proves evidence structure, not taste.

Seal the terminal manifest and every recorded artifact, log, and review image
before revising or reusing the workspace. The sidecar prevents a later capture
from silently replacing the pixels that were scored:

```bash
python3 <skill-root>/scripts/benchmark_run.py seal runs/codex/run.json
```

Validate structure, provenance, and current evidence bytes:

```bash
python3 <skill-root>/scripts/benchmark_run.py validate \
  runs/codex/run.json --require-complete --require-current
```

Use `--require-pass` only when asserting that one run cleared every per-run
gate. A valid failed or blocked run is still benchmark evidence.

## Summarize the two runs

```bash
python3 <skill-root>/scripts/benchmark_run.py summarize \
  runs/codex/run.json \
  runs/cursor/run.json \
  --require-current \
  --output runs/summary.json
```

The first summary should report `awaiting-preference`. After a blind comparison,
record the aggregate preference and its method:

```bash
python3 <skill-root>/scripts/benchmark_run.py summarize \
  runs/codex/run.json runs/cursor/run.json \
  --require-current \
  --pairwise-preference-percent 75 \
  --preference-note "12 blind comparisons against the previous Skill release" \
  --output runs/summary.json
```

Do not report catalog tests, a valid manifest, or an `awaiting-preference`
summary as model or visual-quality success. Publish the prompt, Skill hash,
source commits, run manifests, logs, artifacts, screenshots, scores, blockers,
and comparison method together.
