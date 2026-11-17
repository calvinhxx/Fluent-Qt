"""Verify the real website editor with the shared, already-built WASM payload."""

import importlib.util
from pathlib import Path
import subprocess
import sys


def run_component_playground_contract(browser, _base_url, artifact_dir):
    project = Path(__file__).resolve().parents[2]
    # This renders documents only. Native captures and C++ compilation stay in
    # their existing jobs; the editor itself is tested against the real binary.
    subprocess.run([sys.executable, str(project / 'tools/site/generate_api_reference.py')],
                   cwd=project, check=True)
    path = project / 'tools/site/test_component_playground_browser.py'
    spec = importlib.util.spec_from_file_location('component_playground_browser', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with module.website_server(artifact_dir.parent) as url:
        module.run(browser, url, artifact_dir)
        input_path = project / 'tools/site/test_input_playgrounds_browser.py'
        input_spec = importlib.util.spec_from_file_location('input_playgrounds_browser', input_path)
        inputs = importlib.util.module_from_spec(input_spec)
        input_spec.loader.exec_module(inputs)
        inputs.run(browser, url, artifact_dir / 'inputs', screenshots=True)
