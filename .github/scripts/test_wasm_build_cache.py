#!/usr/bin/env python3
"""Exercise exact inputs, cache integrity and honest build/validation provenance."""

import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
from textwrap import dedent
import unittest

import wasm_build_cache as cache


class WasmBuildCacheTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git('init', '-q')
        self.toolchain = {'QT_VERSION': '6.9.3', 'EMSCRIPTEN_VERSION': '3.1.70',
                          'EMSDK_REPOSITORY_REVISION': 'a' * 40, 'cmake': '3.31.6'}
        for path in ('CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json', 'src/frame.cpp',
                     'app/platform/wasm/shell.html', 'examples/hello_world/main.cpp',
                     'plugin/Spatial/CMakeLists.txt', 'third_party/fonts/LICENSE.txt',
                     '.agents/skills/build-fluentqt-gui/SKILL.md', 'docs/ai/catalog.json',
                     'llms.txt', '.github/workflows/ci-wasm.yml', '.github/scripts/wasm_build_cache.py',
                     'README.md', 'docs/releases/v1.11.4.md', 'site/api/component.js',
                     'tools/site/release-history.json'):
            self.write(path, 'original')
        self.git('add', '.')
        self.inputs = cache.fingerprint(self.root, self.toolchain)
        self.entry = self.root / 'build/wasm-reusable'

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, stderr=subprocess.DEVNULL)

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text)
        return file

    def make_cache(self, mode='full'):
        for path in cache.required_files():
            if path.startswith('sdk/'):
                self.write('install/' + path.removeprefix('sdk/'), 'SDK bytes')
            elif path != 'payload/build-info.json':
                self.write('output/' + path.removeprefix('payload/'), 'payload bytes')
        self.write('output/index.html', 'qtloader.js fluent_qt_gallery.js licenses.html app-icon.png')
        with contextlib.redirect_stdout(io.StringIO()):
            cache.pack(self.entry, self.root / 'output', self.root / 'install', self.inputs,
                       'a' * 40, '6.9.3', '3.1.70', mode)
        return cache.verify(self.entry, self.inputs)

    def test_website_and_release_prose_do_not_change_build_identity(self):
        for path in ('README.md', 'docs/releases/v1.11.4.md', 'site/api/component.js',
                     'tools/site/release-history.json'):
            with self.subTest(path=path):
                self.write(path, 'updated publication state or website prose')
                self.assertEqual(cache.fingerprint(self.root, self.toolchain), self.inputs)

    def test_complete_runtime_sdk_and_toolchain_inputs_invalidate(self):
        for path in self.inputs['inputs']['files']:
            with self.subTest(path=path):
                self.write(path, 'changed')
                self.assertNotEqual(cache.fingerprint(self.root, self.toolchain)['digest'], self.inputs['digest'])
                self.write(path, 'original')
        for name in self.toolchain:
            with self.subTest(tool=name):
                changed = dict(self.toolchain, **{name: 'new version'})
                self.assertNotEqual(cache.fingerprint(self.root, changed)['digest'], self.inputs['digest'])

    def test_added_removed_and_executable_files_invalidate(self):
        self.write('new-owner/new-source.cpp', 'source')
        self.git('add', 'new-owner')
        self.assertNotEqual(cache.fingerprint(self.root, self.toolchain)['digest'], self.inputs['digest'])
        self.git('rm', '-f', 'new-owner/new-source.cpp')
        self.assertEqual(cache.fingerprint(self.root, self.toolchain)['digest'], self.inputs['digest'])
        self.git('update-index', '--chmod=+x', 'src/frame.cpp')
        self.assertNotEqual(cache.fingerprint(self.root, self.toolchain)['digest'], self.inputs['digest'])
        self.git('rm', '-f', 'src/frame.cpp')
        self.assertNotEqual(cache.fingerprint(self.root, self.toolchain)['digest'], self.inputs['digest'])

    def test_incomplete_inputs_and_symlinks_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'complete toolchain'):
            cache.fingerprint(self.root, {'QT_VERSION': ''})
        (self.root / 'src/frame.cpp').unlink()
        (self.root / 'src/frame.cpp').symlink_to(self.root / 'README.md')
        with self.assertRaisesRegex(ValueError, 'Non-regular'):
            cache.fingerprint(self.root, self.toolchain)

    def test_restored_build_keeps_original_commit_and_binary_bytes(self):
        manifest = self.make_cache(mode='fast')
        stage = cache.staging_module()
        before = cache.digest(self.entry / 'payload/fluent_qt_gallery.wasm')
        destination = self.root / 'pages'
        with contextlib.redirect_stdout(io.StringIO()):
            stage.stage(argparse.Namespace(source=self.entry / 'payload', destination=destination,
                qt_version='6.9.3', emscripten_version='3.1.70', validation_mode='full',
                commit='b' * 40, build_commit=manifest['build_commit'],
                build_inputs_sha256=self.inputs['digest']))
        metadata = json.loads((destination / 'build-info.json').read_text())
        self.assertEqual(metadata['commit'], 'b' * 40)
        self.assertEqual(metadata['build_commit'], 'a' * 40)
        self.assertEqual(metadata['validation_mode'], 'full')
        self.assertEqual(cache.digest(destination / 'fluent_qt_gallery.wasm'), before)

    def test_cache_mismatch_corruption_missing_and_extra_files_are_rejected(self):
        for damage in ('inputs', 'binary', 'sdk', 'missing', 'extra', 'permissions', 'identity'):
            with self.subTest(damage=damage):
                self.make_cache()
                expected = json.loads(json.dumps(self.inputs))
                binary = self.entry / 'payload/fluent_qt_gallery.wasm'
                if damage == 'inputs':
                    expected['digest'] = '0' * 64
                elif damage == 'binary':
                    binary.write_text('damaged')
                elif damage == 'sdk':
                    (self.entry / 'sdk/lib/libFluentQt.a').write_text('damaged')
                elif damage == 'missing':
                    binary.unlink()
                elif damage == 'extra':
                    (self.entry / 'payload/unexpected.js').write_text('extra')
                elif damage == 'permissions':
                    binary.chmod(0o755)
                else:
                    path = self.entry / cache.MANIFEST
                    data = json.loads(path.read_text()); data['build_commit'] = 'unverified'
                    path.write_bytes(cache.encoded(data))
                with self.assertRaises(ValueError):
                    cache.verify(self.entry, expected)

    @unittest.skipUnless(shutil.which('bash'), 'Workflow shell needs Bash')
    def test_workflow_reuses_only_verified_hits_and_rebuilds_bad_entries(self):
        self.make_cache()
        self.write('build/wasm-cache-inputs.json', json.dumps(self.inputs))
        workflow = (cache.ROOT / '.github/workflows/ci-wasm.yml').read_text()
        step = workflow.split('      - name: Verify restored WebAssembly build\n', 1)[1].split('      - name:', 1)[0]
        script = dedent(step.split('        run: |\n', 1)[1])
        script = script.replace('python .github/scripts/wasm_build_cache.py',
                                shlex.quote(sys.executable) + ' ' + shlex.quote(str(Path(cache.__file__).resolve())))
        for state, restored, expected in (('valid', 'true', 'true'), ('missing', 'false', 'false'),
                                          ('corrupt', 'true', 'false')):
            with self.subTest(state=state):
                if state == 'corrupt':
                    (self.entry / 'sdk/lib/libFluentQt.a').write_text('corrupt')
                output = self.root / f'{state}.output'
                result = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c',
                    script.replace('${{ steps.wasm-cache.outputs.cache-hit }}', restored)],
                    cwd=self.root, env=dict(os.environ, GITHUB_OUTPUT=str(output)), capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(output.read_text().strip(), 'usable=' + expected)

    def test_missing_required_file_cannot_be_hidden_by_rewriting_manifest(self):
        manifest = self.make_cache()
        required = 'sdk/lib/libFluentQt.a'
        (self.entry / required).unlink()
        del manifest['files'][required]
        (self.entry / cache.MANIFEST).write_bytes(cache.encoded(manifest))
        with self.assertRaisesRegex(ValueError, 'inventory'):
            cache.verify(self.entry, self.inputs)

    def test_cache_and_manifest_symlinks_are_rejected(self):
        self.make_cache()
        alias = self.root / 'alias'
        alias.symlink_to(self.entry, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlinks'):
            cache.verify(alias, self.inputs)
        record = self.entry / cache.MANIFEST
        record.rename(self.root / 'outside.json')
        record.symlink_to(self.root / 'outside.json')
        with self.assertRaisesRegex(ValueError, 'symlinks'):
            cache.verify(self.entry, self.inputs)

    def test_cached_symlink_is_rejected_before_following_it(self):
        self.make_cache()
        (self.entry / 'sdk/link').symlink_to(self.root / 'README.md')
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            cache.verify(self.entry, self.inputs)

    def test_bad_provenance_and_input_document_are_rejected(self):
        stage = cache.staging_module()
        self.make_cache()
        with self.assertRaisesRegex(ValueError, 'Build provenance'):
            stage.stage(argparse.Namespace(source=self.entry / 'payload', destination=self.root / 'pages',
                qt_version='6.9.3', emscripten_version='3.1.70', validation_mode='full',
                commit='b' * 40, build_commit='bad', build_inputs_sha256=self.inputs['digest']))
        self.assertFalse((self.root / 'pages').exists())
        path = self.write('inputs.json', json.dumps(self.inputs))
        self.assertEqual(cache.load_inputs(path), self.inputs)
        data = json.loads(path.read_text()); data['inputs']['toolchain']['QT_VERSION'] = 'new'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'fingerprint'):
            cache.load_inputs(path)


if __name__ == '__main__':
    unittest.main()
