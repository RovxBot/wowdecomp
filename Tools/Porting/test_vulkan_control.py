"""Verify artifact admission and strict environment without compiling or running a GPU."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location('vulkan_control', Path(__file__).parents[1] / 'NativeD3D9/vulkan_control.py')
CONTROL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONTROL)


class VulkanControlTests(unittest.TestCase):
    def artifact(self, directory, strict=False):
        artifact = Path(directory).resolve()
        elf = bytearray(20)
        elf[:6] = b'\x7fELF\x02\x01'
        elf[18:20] = b'\x3e\x00'
        (artifact / 'VulkanLifetimeCheck').write_bytes(elf)
        if strict:
            (artifact / 'lib').mkdir()
            (artifact / 'lib/libclang_rt.asan-x86_64.so').write_bytes(elf)
            (artifact / 'Clang-compiler-rt-copyright').write_text('fixture notice')
        manifest = dict(status='build-and-contracts-passed', kind='VULKAN_LIFETIME_CONTROL',
                        target_os='LINUX', target_cpu='X86_64', sanitizers=strict,
                        files={str(p.relative_to(artifact)): CONTROL.digest(p) for p in artifact.rglob('*') if p.is_file()})
        (artifact / 'manifest.json').write_text(json.dumps(manifest))
        return artifact, manifest

    def test_configuration_and_failed_hosted_results_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact, manifest = self.artifact(directory)
            CONTROL.verify(artifact, False)
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, True)
            manifest['status'] = 'failed'
            (artifact / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, False)

    def test_modified_binary_and_injected_library_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact, _ = self.artifact(directory)
            (artifact / 'lib').mkdir()
            injected = artifact / 'lib/injected.so'
            injected.write_bytes(b'not admitted')
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, False)
            injected.unlink()
            (artifact / 'VulkanLifetimeCheck').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, False)

    def test_recorded_hash_cannot_escape_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / 'artifact'
            artifact.mkdir()
            artifact, manifest = self.artifact(artifact)
            outside = root / 'outside'
            outside.write_text('outside')
            (artifact / 'link').symlink_to(outside)
            manifest['files']['link'] = CONTROL.digest(outside)
            (artifact / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, False)

    def test_strict_runtime_and_notice_required(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact, manifest = self.artifact(directory, strict=True)
            CONTROL.verify(artifact, True)
            (artifact / 'lib/libclang_rt.asan-x86_64.so').unlink()
            del manifest['files']['lib/libclang_rt.asan-x86_64.so']
            (artifact / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                CONTROL.verify(artifact, True)

    def test_detectors_and_ordinary_driver_unload_cannot_be_overridden(self):
        with patch.dict(os.environ, {'LD_PRELOAD': 'shim', 'LD_AUDIT': 'shim', 'LSAN_OPTIONS': 'suppressions=file',
                                     'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=0',
                                     'VK_DRIVER_FILES': '/selected/icd.json'}):
            environment = CONTROL.clean_environment(Path('/verified/lib'))
        for name in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS']:
            self.assertNotIn(name, environment)
        self.assertEqual(environment['ASAN_OPTIONS'], 'detect_leaks=1:halt_on_error=1')
        self.assertEqual(environment['UBSAN_OPTIONS'], 'halt_on_error=1:print_stacktrace=1')
        self.assertEqual(environment['VK_DRIVER_FILES'], '/selected/icd.json')
        self.assertEqual(environment['LD_LIBRARY_PATH'], '/verified/lib')

    def test_available_host_symbolizer_replaces_stale_build_host_path(self):
        with patch.dict(os.environ, {'ASAN_SYMBOLIZER_PATH': '/absent/build-host/path'}), \
                patch.object(CONTROL.shutil, 'which', return_value='/usr/bin/llvm-symbolizer'):
            environment = CONTROL.clean_environment(Path('/verified/lib'))
        self.assertEqual(environment['ASAN_SYMBOLIZER_PATH'], '/usr/bin/llvm-symbolizer')


if __name__ == '__main__':
    unittest.main()
