# Cross-agent benchmark

Use this only when evaluating the Skill itself. It is not part of the normal
GUI build workflow.

## Fixed inputs

- Package the Skill once and give the same archive, without local edits, to
  Codex, Claude Code, and Cursor.
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

Repeat with `claude-code` and `cursor`. Keep artifact and log paths relative to
the run manifest so the run directory can be archived or published intact.

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

Validate structure and provenance:

```bash
python3 <skill-root>/scripts/benchmark_run.py validate \
  runs/codex/run.json --require-complete
```

Use `--require-pass` only when asserting that one run cleared every per-run
gate. A valid failed or blocked run is still benchmark evidence.

## Summarize the three runs

```bash
python3 <skill-root>/scripts/benchmark_run.py summarize \
  runs/codex/run.json \
  runs/claude-code/run.json \
  runs/cursor/run.json \
  --output runs/summary.json
```

The first summary should report `awaiting-preference`. After a blind comparison,
record the aggregate preference and its method:

```bash
python3 <skill-root>/scripts/benchmark_run.py summarize \
  runs/codex/run.json runs/claude-code/run.json runs/cursor/run.json \
  --pairwise-preference-percent 75 \
  --preference-note "12 blind comparisons against the previous Skill release" \
  --output runs/summary.json
```

Do not report catalog tests, a valid manifest, or an `awaiting-preference`
summary as model or visual-quality success. Publish the prompt, Skill hash,
source commits, run manifests, logs, artifacts, screenshots, scores, blockers,
and comparison method together.
