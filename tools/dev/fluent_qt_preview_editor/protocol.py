"""Editor tool descriptions; importing these never loads Qt."""


def editor_tools(tool):
    region = {"type": "string", "minLength": 1, "description": "Registered region key from preview_editor_inspect."}
    return [
        tool("preview_editor_inspect", "Read selected region, registered source locations, fields and review state. Python --edit only."),
        tool("preview_editor_select", "Select and reveal a registered region. Finish pending edits first.",
             {"region": region}, ("region",)),
        tool("preview_editor_apply", "Preview literal source edits in the selected region, preserving UI state. Read current revision first; review before keeping.",
             {"region": region, "revision": {"type": "string", "minLength": 1},
              "changes": {"type": "object", "minProperties": 1, "maxProperties": 32,
                          "additionalProperties": {"type": ["string", "integer", "boolean"]}}},
             ("region", "revision", "changes")),
        tool("preview_editor_compare", "Read the last edit's actual before/after region screenshots and exact source diff."),
        tool("preview_editor_accept", "Keep the pending source edit after reviewing its result."),
        tool("preview_editor_revert", "Revert the pending source edit; refuses to overwrite external changes."),
        tool("preview_editor_undo", "Undo the last kept edit when its source still matches; keeps interaction state."),
        tool("preview_editor_dismiss", "End a pending comparison without changing the current source, for external-edit conflicts."),
    ]
