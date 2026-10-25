"""Shared full-example mapping for the live Gallery and generated catalogs."""

from importlib import import_module


_FACTORIES = {
    "command-palette-shared-actions": ("navigation", "command_palette_source_files"),
    "field-connection-form": ("layout", "connection_form_source_files"),
    "tag-input-project-members": ("text_window", "member_selection_source_files"),
    "tree-select-project-teams": ("basic", "team_selection_source_files"),
    "auto-suggest-box-async": ("text_window", "async_search_source_files"),
    "data-grid-tool-bar-basic": ("collections", "data_grid_source_files"),
    "list-detail-file-browser": ("layout", "file_browser_source_files"),
    "property-panel-chart-workbench": ("property_panel", "workbench_source_files"),
    "popup-modal-dim": ("overlays", "overlay_source_files"),
    "dialog-backdrop-effects": ("overlays", "overlay_source_files"),
    "drawer-view-backdrop-effects": ("overlays", "overlay_source_files"),
}


def source_files_for_sample(sample_id, result):
    factory = _FACTORIES.get(sample_id)
    if factory is None:
        return None
    module, name = factory
    return getattr(import_module(f".native_samples_{module}", __package__), name)(result)


def complete_sample_source_files(result, filename, *, root_name="panel"):
    """Add application setup and an explicit entry point to a preview script."""
    full = result.preview_source.replace(
        "import fluentqt\n", "import fluentqt\n"
        "from PySide6.QtWidgets import QApplication\n"
        "application = QApplication.instance()\n"
        "if application is None:\n"
        "    fluentqt.prepare_high_dpi_application()\n"
        "    application = QApplication([])\n"
        "    fluentqt.initialize_resources()\n"
        "    application.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))\n", 1
    )
    full += (f'\nif __name__ == "__main__":\n    {root_name}.show()\n'
             '    application.exec()\n')
    return [{"name": filename, "language": "python", "code": full,
             "usage_snippet": result.source}]
