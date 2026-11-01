"""DXVK client artifact contracts; no client/dependency compilation."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from run_native_client import verify_artifact


class D3d9ClientPackageTests(unittest.TestCase):
    def package(self, root):
        elf = bytearray(32)
        elf[:6] = b'\x7fELF\x02\x01'
        elf[18:20] = b'\x3e\x00'
        files = {}
        for name in ['WowNativeClient', 'lib/libSDL3.so.0', 'lib/libdxvk_d3d9.so.0', 'notices/DXVK-LICENSE']:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(elf if 'notices/' not in name else b'authored notice')
            files[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest = dict(status='build-and-contracts-passed', preset='linux-client-d3d9',
                        target_os='LINUX', target_cpu='X86_64', signatures='required',
                        backend='DXVK_NATIVE_SDL3', files=files)
        (root / 'manifest.json').write_text(json.dumps(manifest))
        return manifest

    def test_explicit_backend_and_matching_mode(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.package(root)
            self.assertEqual(verify_artifact(root, False, 'dxvk')[1], root / 'WowNativeClient')
            for mode, renderer in [(True, 'dxvk'), (False, 'opengl')]:
                with self.assertRaises(ValueError):
                    verify_artifact(root, mode, renderer)

    def test_all_runtime_bytes_and_notices_are_verified(self):
        for name in ['WowNativeClient', 'lib/libSDL3.so.0', 'lib/libdxvk_d3d9.so.0', 'notices/DXVK-LICENSE']:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                self.package(root)
                (root / name).write_bytes(b'changed')
                with self.assertRaises(ValueError):
                    verify_artifact(root, False, 'dxvk')

    def test_unrecorded_library_and_manifest_escape_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest = self.package(root)
            (root / 'lib/unrecorded.so').write_bytes((root / 'WowNativeClient').read_bytes())
            with self.assertRaises(ValueError):
                verify_artifact(root, False, 'dxvk')
            (root / 'lib/unrecorded.so').unlink()
            manifest['files']['../escape'] = '0' * 64
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                verify_artifact(root, False, 'dxvk')


if __name__ == '__main__':
    unittest.main()
