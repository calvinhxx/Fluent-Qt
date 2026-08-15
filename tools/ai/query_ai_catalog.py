#!/usr/bin/env python3

"""Compatibility entry point for the installable FluentQt GUI Skill query."""

from __future__ import annotations

from pathlib import Path
import sys


SKILL_SCRIPTS = (
    Path(__file__).resolve().parents[2]
    / ".agents"
    / "skills"
    / "build-fluentqt-gui"
    / "scripts"
)
sys.path.insert(0, str(SKILL_SCRIPTS))

from query_catalog import (  # noqa: E402,F401
    BUNDLED_CATALOG,
    component_by_id,
    format_component,
    format_guide,
    format_pattern,
    guide_by_id,
    load_catalog,
    main,
    parse_args,
    pattern_by_id,
    search_components,
)


if __name__ == "__main__":
    raise SystemExit(main())
