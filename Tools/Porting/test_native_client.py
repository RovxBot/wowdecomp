"""Integrated startup artifact/path policies; no client compilation."""
import json
from pathlib import Path
import tempfile
import unittest
from run_native_client import validate_paths, verify_artifact


class NativeClientPolicyTests(unittest.TestCase):
    def test_reference_installations_are_protected(self):
        for path in ['/mnt/games/3.3.5aClean/WTF', '/mnt/games/ChromieCraft/Logs']:
            with self.assertRaises(ValueError):
                validate_paths(Path('/mnt/games/3.3.5aClean/Data'), Path(path), Path('/tmp/client-cache'), Path('/tmp/client.json'))

    def test_symlink_and_overlapping_roots_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'game/Data').mkdir(parents=True)
            (root / 'settings').symlink_to(root / 'game')
            with self.assertRaises(ValueError):
                validate_paths(root / 'game/Data', root / 'settings', root / 'cache', root / 'report.json')
            with self.assertRaises(ValueError):
                validate_paths(root / 'game/Data', root / 'user', root / 'user/cache', root / 'report.json')

    def test_failed_build_and_wrong_sanitizer_mode_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest = dict(status='failed', preset='linux-client', target_os='LINUX', target_cpu='X86_64', signatures='required')
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                verify_artifact(root, False)
            manifest['status'] = 'build-and-contracts-passed'
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                verify_artifact(root, True)


if __name__ == '__main__':
    unittest.main()
