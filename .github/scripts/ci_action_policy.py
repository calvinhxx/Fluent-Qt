"""Central Action pinning policy; concrete revisions live only in workflow YAML."""

from __future__ import annotations

from collections import Counter
import re


# Keep the allowed repositories and their invocation counts independent of the
# revisions maintained by Dependabot. Deployment inputs are checked separately.
PAGES_PIPELINE_ACTIONS = {
    "ci-site.yml": {
        "actions/checkout": 1,
        "actions/setup-python": 1,
        "actions/cache": 2,
        "actions/upload-artifact": 2,
    },
    "pages.yml": {
        "actions/checkout": 1,
        "actions/configure-pages": 1,
        "actions/download-artifact": 2,
        "actions/upload-pages-artifact": 1,
        "actions/deploy-pages": 1,
    },
    "ci-wasm.yml": {
        "actions/checkout": 1,
        "actions/setup-python": 1,
        "actions/cache": 5,
        "actions/upload-artifact": 3,
    },
}
ACTION_USES = re.compile(
    r"^(?P<prefix>[ \t]+(?:-[ \t]+)?uses:[ \t]*)(?P<value>[^\n]*)$",
    re.MULTILINE,
)
PINNED_REMOTE_ACTION = re.compile(
    r"(?P<action>[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)+)@[0-9a-fA-F]{40}"
)
PINNED_CONTAINER_ACTION = re.compile(r"docker://[^\s@]+@sha256:[0-9a-f]{64}")


def action_reference(value: str) -> str:
    """Read a plain or quoted uses value, ignoring its version comment."""
    value = re.split(r"\s+#", value, maxsplit=1)[0].strip()
    if value[:1] in {"'", '"'} and value[-1:] == value[:1]:
        value = value[1:-1]
    return value


def immutable_action_errors(name: str, contents: str) -> list[str]:
    """Require full commit SHAs or container digests for every remote action."""
    errors: list[str] = []
    for match in ACTION_USES.finditer(contents):
        value = action_reference(match.group("value"))
        if value.startswith("./"):
            continue
        if value.startswith("docker://"):
            valid = PINNED_CONTAINER_ACTION.fullmatch(value)
        else:
            valid = PINNED_REMOTE_ACTION.fullmatch(value)
        if not valid:
            errors.append(
                f"{name} must pin remote action to a full commit SHA or image digest: {value}"
            )
    return errors


def required_action_errors(name: str, contents: str) -> list[str]:
    """Keep the Pages chain on its allowed actions and exact invocation counts."""
    errors = immutable_action_errors(name, contents)
    expected_actions = PAGES_PIPELINE_ACTIONS.get(name, {})
    counts: Counter[str] = Counter()
    for match in ACTION_USES.finditer(contents):
        reference = action_reference(match.group("value"))
        if reference.startswith("./"):
            continue
        action = reference.rsplit("@", 1)[0]
        counts[action] += 1
        if action not in expected_actions:
            errors.append(
                f"{name} contains an unaudited remote action in the Pages chain: "
                f"{reference}"
            )
    for action, expected_count in expected_actions.items():
        if counts[action] != expected_count:
            errors.append(
                f"{name} must use {action} exactly {expected_count} time(s)"
            )
    return errors


def normalize_action_references(contents: str) -> str:
    """Ignore valid SHA updates in step comparisons, retaining action identity.

    Callers also run immutable_action_errors on the original contents. The
    placeholder is comparison-only and must never be written to a workflow.
    """
    def normalize(match: re.Match[str]) -> str:
        reference = action_reference(match.group("value"))
        pinned = PINNED_REMOTE_ACTION.fullmatch(reference)
        if pinned is None:
            return match.group(0)
        return f"{match.group('prefix')}{pinned.group('action')}@<commit>"

    return ACTION_USES.sub(normalize, contents)
