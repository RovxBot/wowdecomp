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

    def test_gl_selects_common_legacy_dispatch_and_rejects_wgl(self):
        message = 'WowNativeGl: unexpected native Gx context provider/ABI'
        target = self.target('WowNativeGl', 'Engine/Source/Gx/Portable/GlContext.cpp')
        target['sources'] += [{'path': 'Engine/Source/Gx/Portable/' + name + '.cpp'}
                              for name in ['GlBuffers', 'GlProgram', 'GlRenderTargets', 'GlTransforms', 'ShaderPackage']]
        target['sources'] += [{'path': path} for path in [
            'Engine/Source/Gx/CGxMatrixStack.cpp', 'Engine/Source/Tempest/C44Matrix.cpp',
            'Engine/Source/Tempest/C33Matrix.cpp'] + [
            'Engine/Source/Gx/Portable/LegacyDevice.cpp',
            'Engine/Source/Gx/Gx.cpp',
            'Engine/Source/Gx/Buffer.cpp',
            'Engine/Source/Gx/CGxDevice/CGxDevice.cpp',
            'Engine/Source/Gx/CGxDevice/CGxShader.cpp',
            'Engine/Source/Gx/CGxDevice/EmergencyMem.cpp',
            'Engine/Source/Gx/CGxTex.cpp',
            'Engine/Source/Gx/CGxTexFlags.cpp',
            'Engine/Source/Gx/CGxPool.cpp',
            'Engine/Source/Gx/CGxBuf.cpp',
            'Engine/Source/Gx/CGxFormat.cpp',
            'Engine/Source/Gx/CGxGammaRamp.cpp',
            'Engine/Source/Tempest/CiRect.cpp',
            'Engine/Source/Tempest/CRect.cpp',
            'Engine/Source/Gx/CGxLight.cpp',
            'Engine/Source/Tempest/C2Vector.cpp',
            'Engine/Source/Tempest/C4Plane.cpp',
            ]]
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        legacy = copy.deepcopy(target)
        legacy['sources'].append({'path': 'Engine/Source/Gx/CGxDeviceOpenGL/CGxDeviceOpenGl.cpp'})
        self.assertIn(message, audit([legacy], 'gl', 'LINUX', 'X86_64'))
        target['compileGroups'][0]['defines'].pop()
        self.assertIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))

    def test_frame_fonts_reject_unadapted_region_and_model_sources(self):
        target = self.target('WowNativeFrameFonts', 'Engine/Source/Frame/CSimpleFont.cpp')
        target['sources'] = [{'path': 'Engine/Source/Frame/' + name + '.cpp'} for name in
            ['CScriptObject', 'CScriptObject_Scripts', 'CreateFont', 'CSimpleFont',
             'CSimpleFont_Scripts', 'CSimpleFontable', 'CSimpleFontStringAttributes',
             'ScriptColor', 'Portable/Fonts']]
        target['sources'] += [{'path': path} for path in
            ['Engine/Source/FrameXML/FontXML.cpp', 'Engine/Source/Tempest/CImVector.cpp']]
        message = 'WowNativeFrameFonts: unexpected native Frame font source/ABI'
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        for path in ['Engine/Source/Frame/CSimpleRender.cpp', 'Engine/Source/Frame/CSimpleModel.cpp']:
            changed = copy.deepcopy(target)
            changed['sources'].append({'path': path})
            self.assertIn(message, audit([changed], 'gl', 'LINUX', 'X86_64'))

    def test_gl_rejects_signature_bypass_and_windows_definitions(self):
        target = self.target('NativeGlCheck', 'Tools/NativeGx/main.cpp')
        message = 'NativeGlCheck: Windows/client definitions leaked into core'
        for definition in ['DISABLE_SIGNATURE_CHECKS', 'WIN32=1', '_WINDOWS']:
            modified = copy.deepcopy(target)
            modified['compileGroups'][0]['defines'].append({'define': definition})
            self.assertIn(message, audit([modified], 'gl', 'LINUX', 'X86_64'))

    def test_frame_renderer_selects_bounded_batch_sources(self):
        target = self.target('WowNativeFrameRender', 'Engine/Source/Frame/CRenderBatch.cpp')
        target['sources'] += [{'path': 'Engine/Source/Frame/' + name + '.cpp'}
                              for name in ['CSimpleBatchRender', 'Portable/Render']]
        message = 'WowNativeFrameRender: unexpected native Frame renderer source/ABI'
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        for name in ['CSimpleRender', 'CSimpleFrame', 'CSimpleTop']:
            changed = copy.deepcopy(target)
            changed['sources'].append({'path': 'Engine/Source/Frame/' + name + '.cpp'})
            self.assertIn(message, audit([changed], 'gl', 'LINUX', 'X86_64'))

    def test_regions_select_root_and_reject_unselected_script_factories(self):
        target = self.target('WowNativeFrameRegions', 'Engine/Source/Frame/CScriptRegion.cpp')
        target['sources'] += [{'path': 'Engine/Source/Frame/' + name + '.cpp'} for name in
            ['CSimpleRegion', 'CSimpleFontString', 'FontStringAttributes', 'FramePixel', 'CSimpleFrame', 'CSimpleTop', 'CFrameStrataNode', 'Portable/Hierarchy', 'Portable/WindowEvents']]
        target['sources'] += [{'path': path} for path in ['Engine/Source/Services/ScreenProjection.cpp', 'Engine/Source/Event/CEvent.cpp', 'Engine/Source/Event/CMouseEvent.cpp']]
        message = 'WowNativeFrameRegions: unexpected native Frame region source/ABI'
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        for name in ['CSimpleFrame_Scripts', 'CSimpleTexture', 'CSimpleFontString_Scripts', 'CSimpleAnimGroup']:
            changed = copy.deepcopy(target)
            changed['sources'].append({'path': 'Engine/Source/Frame/' + name + '.cpp'})
            self.assertIn(message, audit([changed], 'gl', 'LINUX', 'X86_64'))

    def test_gxu_rejects_full_legacy_file_and_texture_services(self):
        message = 'WowNativeGxu: unexpected native Gxu provider/ABI'
        target = self.target('WowNativeGxu', 'Engine/Source/Gxu/Portable/LegacyFonts.cpp')
        target['sources'] = [{'path': 'Engine/Source/Gxu/' + name + '.cpp'} for name in [
            'Portable/LegacyFonts', 'Portable/FontTexture', 'Portable/FontMath', 'Gxu', 'GxuFontFace',
            'GxuFontFont', 'GxuFontMain', 'GxuFontString', 'GxuFontTextureCache',
            'GxuFontMiscClasses', 'GxuFontUtil', 'IGxuFontGlyph']]
        target['sources'] += [{'path': 'Engine/Source/Base/' + name + '.cpp'} for name in
                              ['Handle', 'Status', 'Coordinate', 'ConvertUTF']]
        target['sources'] += [{'path': 'Engine/Source/Tempest/' + name + '.cpp'} for name in
                              ['C3Vector', 'C4Vector']]
        target['sources'].append({'path': 'Engine/Source/Services/TextBlock.cpp'})
        target['sources'].append({'path': 'Engine/Source/Services/Portable/Texture.cpp'})
        self.assertNotIn(message, audit([target], 'gl', 'LINUX', 'X86_64'))
        for source in ['Engine/Source/Services/Texture.cpp',
                       'Engine/Source/BlizzardCore/Source/SFile.cpp']:
            modified = copy.deepcopy(target)
            modified['sources'].append({'path': source})
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

    def test_legacy_probe_uses_its_own_verified_binary(self):
        self.script('raise SystemExit("independent preview must not execute")\n')
        self.target_script('import sys\nassert sys.argv[1:] == []\nprint("legacy-dispatch-passed")\n', 'LegacyGxCheck')
        result = self.run_wrapper('--legacy-device')
        self.assertEqual(result.returncode, 0, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['probe'], 'LegacyGxCheck')
        self.assertEqual(record['stdout'], ['legacy-dispatch-passed'])

    def test_legacy_probe_cannot_reuse_preview_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('raise SystemExit("legacy must not execute")\n', 'LegacyGxCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--legacy-device')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_gxu_probe_uses_its_own_manifest_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        self.target_script('print("original-gxu-passed")\n', 'LegacyGxuCheck')
        result = self.run_wrapper('--legacy-gxu')
        self.assertEqual(result.returncode, 0, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['probe'], 'LegacyGxuCheck')
        self.assertEqual(record['stdout'], ['original-gxu-passed'])

    def test_frame_font_probe_requires_its_own_binary_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('print("frame-fonts-passed")\n', 'NativeFrameFontsCheck')
        result = self.run_wrapper('--frame-fonts')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.output.read_text())['probe'], 'NativeFrameFontsCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-fonts')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_image_probe_requires_its_own_binary_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('import sys\nassert sys.argv[1:] == []\nprint("images-passed")\n', 'NativeImageCheck')
        result = self.run_wrapper('--images')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.output.read_text())['probe'], 'NativeImageCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--images')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_render_probe_requires_its_own_binary_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('print("frame-render-passed")\n', 'NativeFrameRenderCheck')
        result = self.run_wrapper('--frame-render')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.output.read_text())['probe'], 'NativeFrameRenderCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-render')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_regions_probe_requires_its_own_binary_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('print("frame-regions-passed")\n', 'NativeFrameRegionsCheck')
        result = self.run_wrapper('--frame-regions')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.output.read_text())['probe'], 'NativeFrameRegionsCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-regions')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_hierarchy_probe_requires_its_own_binary_hash(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('print("frame-hierarchy-passed")\n', 'NativeFrameHierarchyCheck')
        result = self.run_wrapper('--frame-hierarchy')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.output.read_text())['probe'], 'NativeFrameHierarchyCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-hierarchy')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_mouse_selects_input_mode_and_verifies_binary(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('import sys\nassert sys.argv[1:] == ["--mouse-events"]\n', 'NativeFrameHierarchyCheck')
        result = self.run_wrapper('--frame-mouse')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(self.output.read_text())['frame_mouse_events'])
        result = self.run_wrapper('--frame-mouse', '--frame-ime')
        self.assertNotEqual(result.returncode, 0)
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-mouse')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_ime_selects_input_mode_and_verifies_binary(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('import sys\nassert sys.argv[1:] == ["--ime-events"]\n', 'NativeFrameHierarchyCheck')
        result = self.run_wrapper('--frame-ime')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(self.output.read_text())['frame_ime_events'])
        result = self.run_wrapper('--frame-ime', '--frame-keyboard')
        self.assertNotEqual(result.returncode, 0)
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-ime')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_frame_keyboard_selects_injected_input_mode(self):
        self.script('raise SystemExit("preview must not execute")\n')
        self.target_script('import sys\nassert sys.argv[1:] == ["--keyboard-events"]\n', 'NativeFrameHierarchyCheck')
        result = self.run_wrapper('--frame-keyboard')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(self.output.read_text())['frame_keyboard_events'])
        result = self.run_wrapper('--frame-keyboard', '--frame-window')
        self.assertNotEqual(result.returncode, 0)

    def test_frame_window_selects_event_mode_and_checks_hierarchy_binary(self):
        self.script('raise SystemExit("preview must not execute")\n')
        binary = self.target_script('import sys\nassert sys.argv[1:] == ["--window-events"]\n', 'NativeFrameHierarchyCheck')
        result = self.run_wrapper('--frame-window')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(self.output.read_text())['frame_window_events'])
        result = self.run_wrapper('--frame-window', '--frame-hierarchy')
        self.assertNotEqual(result.returncode, 0)
        binary.write_text('tampered')
        result = self.run_wrapper('--frame-window')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

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

    def test_buffer_failure_recorded_and_fails_combined_run(self):
        self.script('print("probe-passed")\n')
        self.target_script('print("buffer-failed")\nraise SystemExit(6)\n', 'GlBuffersCheck')
        result = self.run_wrapper('--buffer-checks')
        self.assertEqual(result.returncode, 6, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['probe_exit_code'], 0)
        self.assertEqual(record['buffer_checks']['exit_code'], 6)
        self.assertEqual(record['buffer_checks']['stdout'], ['buffer-failed'])

    def test_buffer_hash_checked_before_any_execution(self):
        self.script('raise SystemExit("probe must not execute")\n')
        binary = self.target_script('raise SystemExit("buffers must not execute")\n', 'GlBuffersCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--buffer-checks')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('buffer binary hash does not match', result.stderr)
        self.assertFalse(self.output.exists())

    def test_texture_failure_recorded_and_fails_combined_run(self):
        self.script('print("probe-passed")\n')
        self.target_script('print("texture-failed")\nraise SystemExit(5)\n', 'GlTexturesCheck')
        result = self.run_wrapper('--texture-checks')
        self.assertEqual(result.returncode, 5, result.stderr)
        record = json.loads(self.output.read_text())
        self.assertEqual(record['probe_exit_code'], 0)
        self.assertEqual(record['texture_checks']['exit_code'], 5)
        self.assertEqual(record['texture_checks']['stdout'], ['texture-failed'])

    def test_texture_hash_checked_before_any_execution(self):
        self.script('raise SystemExit("probe must not execute")\n')
        binary = self.target_script('raise SystemExit("textures must not execute")\n', 'GlTexturesCheck')
        binary.write_text('tampered')
        result = self.run_wrapper('--texture-checks')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('texture binary hash does not match', result.stderr)
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
