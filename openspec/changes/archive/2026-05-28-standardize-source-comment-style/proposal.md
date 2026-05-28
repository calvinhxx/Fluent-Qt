## Why

Source comments currently mix Chinese-only notes, partial Doxygen blocks, visual separators, and implementation caveats without a shared rule for public API documentation or bilingual wording. A project-wide comment style will make component contracts easier for maintainers and AI tools to understand while avoiding noisy, stale, mechanically duplicated comments.

## What Changes

- Add a canonical source comment style for code under `src/`, covering public headers, private implementation notes, module-level summaries, terminology, and bilingual usage.
- Document when comments must be bilingual and when a single-language implementation note is acceptable.
- Establish comment density expectations by module type: design tokens, compatibility helpers, foundation infrastructure, reusable widgets, utilities, and view models.
- Update development guidance so future component and infrastructure work follows the same comment rules.
- Apply a small first-pass demonstration to high-signal source files instead of rewriting all existing comments in one change.
- Add a mechanical view-header pass that fills missing class and `Q_PROPERTY` comments across `src/view/**/*.h` without changing behavior.
- Polish the view-header pass so generated comments describe real component/property semantics instead of placeholder wording.
- Polish `src/design/` and `src/compatibility/` comments so token tables and platform/version shims use English anchors plus `zh_CN` explanations.

## Capabilities

### New Capabilities
- `source-comment-style`: Defines the durable requirements for consistent, bilingual-friendly source comments under `src/`.

### Modified Capabilities
- `developer-workflow-docs`: Development workflow docs must include and link to the canonical source comment style guidance.

## Impact

- Adds `docs/development/comment-style.md` and links it from development documentation and agent-facing guidance.
- May update representative headers or implementation comments under `src/design/`, `src/view/foundation/`, and `src/utils/` to demonstrate the style.
- Updates public view headers under `src/view/**/*.h` with mechanical bilingual comments where class or property comments are missing.
- Replaces low-value generated comment phrases with component-specific contracts and property behavior descriptions.
- Updates design token and compatibility comments without changing token values, APIs, or platform behavior.
- Does not change runtime behavior, public APIs, dependencies, or build configuration.
