"""Verify the real website editor with the shared, already-built WASM payload."""

import importlib.util
from pathlib import Path
import subprocess
import sys


PLAYGROUND_GROUPS = ("playground-1", "playground-2", "playground-3")


def playground_selection(routes, group):
    """Assign the shared editor checks once and interleave the remaining component routes."""
    if group == "all":
        return True, routes
    index = PLAYGROUND_GROUPS.index(group)
    return index == 0, routes[index::len(PLAYGROUND_GROUPS)]


def run_component_playground_contract(browser, _base_url, artifact_dir, group="all"):
    project = Path(__file__).resolve().parents[2]
    # This renders documents only. Native captures and C++ compilation stay in
    # their existing jobs; the editor itself is tested against the real binary.
    subprocess.run([sys.executable, str(project / 'tools/site/generate_api_reference.py')],
                   cwd=project, check=True)
    path = project / 'tools/site/test_input_playgrounds_browser.py'
    spec = importlib.util.spec_from_file_location('input_playgrounds_browser', path)
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    editor = inputs.load_browser_contract()
    shared_editor, routes = playground_selection(inputs.registered_routes(), group)
    output = artifact_dir if group == "all" else artifact_dir / group
    with editor.website_server(artifact_dir.parent) as url:
        if shared_editor:
            editor.run(browser, url, output)
        results = inputs.run(browser, url, output / 'inputs', screenshots=True, routes=routes)
        if [result['route'] for result in results] != routes:
            raise AssertionError(f"{group} did not exercise its assigned playground routes")
