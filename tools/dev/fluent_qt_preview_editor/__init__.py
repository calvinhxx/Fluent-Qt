"""Optional source-backed editing for registered Python Live Scene regions.

Scene registration has no Qt import and no dependency on the settings demo.
The controller and visual tools are loaded only by the --edit entry point.
"""

from .registry import Field, register_region

__all__ = ["Field", "register_region"]
