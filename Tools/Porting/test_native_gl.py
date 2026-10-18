"""Source/runtime-launcher policy checks; no client or dependency compilation."""
import copy
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest

from audit_build_graph import audit


class NativeGlGraphTests(unittest.TestCase):
    def target(self, name, source):
        return dict(name=name, type='STATIC_LIBRARY', sources=[{'path': source}],
                    compileGroups=[{'defines': [{'define': definition} for definition in
                        ['WOWDECOMP_OS_LINUX=1', 'WOWDECOMP_CPU_X86_64=1',
                         'WOWDECOMP_ARCH_64=1', 'WOWDECOMP_PORTABLE_RUNTIME=1']]}])

    def test_gl_rejects_legacy_device_sources_and_abi(self):
        message = 'WowNativeGl: unexpected native Gx context provider/ABI'
        target = self.target('WowNativeGl', 'Engine/Source/Gx/Portable/GlContext.cpp')
        target['sources'] += [{'path': 'Engine/Source/Gx/Portable/' + name + '.cpp'}
                              for name in ['GlProgram', 'GlRenderTargets', 'GlTransforms', 'ShaderPackage']]
        target['sources'] += [{'path': path} for path in [
            'Engine/Source/Gx/CGxMatrixStack.cpp', 'Engine/Source/Tempest/C44Matrix.cpp',
            'Engine/Source/Tempest/C33Matrix.cpp']]
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        legacy = copy.deepcopy(target)
        legacy['sources'].append({'path': 'Engine/Source/Gx/CGxDeviceOpenGL/CGxDeviceOpenGl.cpp'})
        self.assertIn(message, audit([legacy], 'gl', 'LINUX', 'X86_64'))
        target['compileGroups'][0]['defines'].pop()
        self.assertIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))

    def test_gl_rejects_signature_bypass_and_windows_definitions(self):
        target = self.target('NativeGlCheck', 'Tools/NativeGx/main.cpp')
        message = 'NativeGlCheck: Windows/client definitions leaked into core'
        for definition in ['DISABLE_SIGNATURE_CHECKS', 'WIN32=1', '_WINDOWS']:
            modified = copy.deepcopy(target)
            modified['compileGroups'][0]['defines'].append({'define': definition})
            self.assertIn(message, audit([modified], 'gl', 'LINUX', 'X86_64'))


@unittest.skipUnless(platform.system() == 'Linux' and platform.machine() == 'x86_64',
                     'Linux runtime wrapper')
class NativeGlLauncherTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.artifact = self.root / 'artifact'
        (self.artifact / 'bin').mkdir(parents=True)
        self.binary = self.artifact / 'bin/NativeGlCheck'
        self.output = self.root / 'record.json'
        self.wrapper = Path(__file__).with_name('run_native_gl.py')

    def script(self, source):
        self.binary.write_text('#!' + sys.executable + '\n' + source)
        manifest = dict(preset='linux-gl', target_os='LINUX', source_commit='fixture-source',
                        workflow_run='fixture-run', binaries=[dict(name='NativeGlCheck',
                        sha256=hashlib.sha256(self.binary.read_bytes()).hexdigest())])
        (self.artifact / 'manifest.json').write_text(json.dumps(manifest))

    def run_wrapper(self, *extra):
        return subprocess.run([sys.executable, str(self.wrapper), '--artifact', str(self.artifact),
            '--output', str(self.output), *extra], capture_output=True, text=True)

    def target_script(self, source, name="GlRenderTargetsCheck"):
        binary = self.artifact / 'bin' / name
        binary.write_text('#!' + sys.executable + '\n' + source)
        path = self.artifact / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['binaries'].append(dict(name=name,
                                        sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
        path.write_text(json.dumps(manifest))
        return binary

    def test_target_failure_is_recorded_separately_and_fails_combined_run(self):
        self.script('print("probe-passed")\n')
        self.target_script('print("target-failed")\nraise SystemExit(9)\n')
        result = self.run_wrapper('--render-target-checks')
        self.assertEqual(result.returncode, 9, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['exit_code'], 9)
        self.assertEqual(record['probe_exit_code'], 0)
        self.assertEqual(record['stdout'], ['probe-passed'])
        self.assertEqual(record['render_target_checks']['exit_code'], 9)
        self.assertEqual(record['render_target_checks']['stdout'], ['target-failed'])

    def test_target_hash_checked_before_any_execution(self):
        self.script('raise SystemExit("probe must not execute")\n')
        binary = self.target_script('raise SystemExit("target must not execute")\n')
        binary.write_text('tampered')
        result = self.run_wrapper('--render-target-checks')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('render-target binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_transform_failure_and_target_success_recorded_separately(self):
        self.script('print("probe-passed")\n')
        self.target_script('print("targets-passed")\n')
        self.target_script('print("transform-failed")\nraise SystemExit(8)\n', 'GlTransformsCheck')
        result = self.run_wrapper('--render-target-checks', '--transform-checks')
        self.assertEqual(result.returncode, 8, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['probe_exit_code'], 0)
        self.assertEqual(record['render_target_checks']['exit_code'], 0)
        self.assertEqual(record['transform_checks']['exit_code'], 8)
        self.assertEqual(record['transform_checks']['stdout'], ['transform-failed'])

    def test_transform_hash_checked_before_any_execution(self):
        self.script('raise SystemExit("probe must not execute")\n')
        self.target_script('raise SystemExit("targets must not execute")\n')
        binary = self.target_script('raise SystemExit("transforms must not execute")\n', 'GlTransformsCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--render-target-checks', '--transform-checks')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('transform binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_refuses_tampered_artifact_before_execution(self):
        self.script('raise SystemExit("Should not execute")\n')
        self.binary.write_text('tampered')
        self.output.write_text('{"exit_code": 0}')
        result = self.run_wrapper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_records_failure_and_matching_x11_environment(self):
        self.script('import os\nprint(os.environ["SDL_VIDEODRIVER"], os.environ["EGL_PLATFORM"])\n'
                    'raise SystemExit(7)\n')
        result = self.run_wrapper()
        self.assertEqual(result.returncode, 7, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['exit_code'], 7)
        self.assertEqual(record['stdout'], ['x11 x11'])
        self.assertFalse(record['timed_out'])

    def test_timeout_replaces_old_success_record(self):
        self.script('import time\ntime.sleep(10)\n')
        self.output.write_text('{"exit_code": 0}')
        result = self.run_wrapper('--timeout', '1')
        self.assertEqual(result.returncode, 124, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['exit_code'], 124)
        self.assertTrue(record['timed_out'])

    def test_outputs_and_user_roots_cannot_write_to_installation(self):
        self.script('raise SystemExit("Should not execute")\n')
        installation = self.root / 'game'
        result = self.run_wrapper('--data', str(installation / 'Data'), '--settings',
            str(self.root / 'settings'), '--cache', str(self.root / 'cache'),
            '--snapshot', str(installation / 'image.ppm'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('outside the game installation', result.stderr)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
