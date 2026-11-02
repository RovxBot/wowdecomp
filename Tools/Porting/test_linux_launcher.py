"""Desktop discovery/configuration contracts; no local client compilation."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SOURCE / 'Tools/NativeD3D9'))
from launch import discover_data, read_endpoint, runtime_environment, seed_realmlist, user_file, user_paths
from package import desktop_entry, linked_libraries, stage_launchers
from run_native_client import validate_paths


class LinuxLauncherTests(unittest.TestCase):
    def test_runtime_closure_parses_relocated_paths_with_spaces(self):
        with patch('package.subprocess.check_output', return_value='\tlibSDL3.so.0 => /game/WoW Native/lib/libSDL3.so.0 (0x1234)\n'):
            self.assertEqual(linked_libraries(Path('/client'), {}), {'libSDL3.so.0': Path('/game/WoW Native/lib/libSDL3.so.0')})
        with patch('package.subprocess.check_output', return_value='\tlibSDL3.so.0 => not found\n'):
            with self.assertRaisesRegex(RuntimeError, 'Unresolved'):
                linked_libraries(Path('/client'), {})

    def test_discovery_beside_data_and_one_directory_below(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'game/dAtA/EnUs').mkdir(parents=True)
            (root / 'game/native').mkdir()
            for package in [root / 'game', root / 'game/native']:
                self.assertEqual(discover_data(package), (root / 'game/dAtA').resolve())
            self.assertEqual(discover_data(root, root / 'game/dAtA'), (root / 'game/dAtA').resolve())
            with self.assertRaisesRegex(ValueError, 'Cannot find Data'):
                discover_data(root)
            (root / 'other/Data').mkdir(parents=True)
            with self.assertRaisesRegex(ValueError, 'enUS'):
                discover_data(root / 'other')

    @unittest.skipIf(os.name == 'nt', 'Distinct case siblings require a case-sensitive host')
    def test_ambiguous_data_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'Data').mkdir()
            (root / 'data').mkdir()
            if len(list(root.iterdir())) < 2:
                self.skipTest('Case-insensitive filesystem')
            with self.assertRaisesRegex(ValueError, 'Ambiguous'):
                discover_data(root)

    def test_profiles_and_installations_have_stable_separate_xdg_state(self):
        base = Path(tempfile.gettempdir()).resolve()
        environment = dict(XDG_CONFIG_HOME=str(base / 'config'), XDG_CACHE_HOME=str(base / 'cache'), XDG_STATE_HOME=str(base / 'state'))
        identities = []
        for data in [Path('/game-one/Data'), Path('/game-two/Data')]:
            for test_build in [False, True]:
                for strict in [False, True]:
                    manifest = dict(test_server_login=test_build, preset='linux-client-d3d9' + ('-asan' if strict else ''))
                    paths = user_paths(data, manifest, environment)
                    self.assertEqual(len(set(paths)), 3)
                    identities.append(paths)
        self.assertEqual(len(set(identities)), 8)
        with self.assertRaisesRegex(ValueError, 'absolute'):
            user_paths(Path('/game/Data'), manifest, dict(XDG_CONFIG_HOME='relative'))

    def test_realmlist_only_seed_external_override_and_idempotency(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            data = root / 'game/Data'
            (data / 'enUS').mkdir(parents=True)
            source = data / 'enUS/realmlist.wtf'
            original = 'SET realmlist "first.invalid"\nset REALMLIST isolated.invalid:3724 // endpoint\nSET accountName "private"\nSET readTOS "1"\n'
            source.write_text(original)
            settings = root / 'external'
            seed_realmlist(data, settings)
            config = settings / 'WTF/Config.wtf'
            self.assertEqual(config.read_text().strip(), 'SET realmlist "isolated.invalid:3724"')
            source.write_text('set realmlist "new.invalid"\n')
            seed_realmlist(data, settings)
            self.assertEqual(read_endpoint(config), 'isolated.invalid:3724')
            config.write_text('SET realmlist ""\n')
            seed_realmlist(data, settings)
            self.assertEqual(config.read_text(), 'SET realmlist ""\n')
            self.assertEqual(source.read_text(), 'set realmlist "new.invalid"\n')
            source.unlink()
            config.unlink()
            (data.parent / 'realmlist.wtf').write_text('\ufeffset realmlist "root.invalid"\n')
            seed_realmlist(data, settings)
            self.assertEqual(read_endpoint(config), 'root.invalid')

    def test_realmlist_refuses_commands_and_invalid_ports(self):
        for endpoint in ['"host"; SET readTOS "1"', '"host:0"', '"host:65536"', '"https://host"']:
            with self.subTest(endpoint=endpoint), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                (root / 'game/Data/enUS').mkdir(parents=True)
                (root / 'game/Data/enUS/realmlist.wtf').write_text('SET realmlist ' + endpoint)
                with self.assertRaises(ValueError):
                    seed_realmlist(root / 'game/Data', root / 'settings')
                self.assertFalse((root / 'settings').exists())

    @unittest.skipIf(os.name == 'nt', 'Symlink creation may require Windows privileges')
    def test_writable_symlinks_cannot_escape_to_installation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'game/Data').mkdir(parents=True)
            (root / 'game/WTF').mkdir()
            (root / 'settings').mkdir()
            (root / 'settings/WTF').symlink_to(root / 'game/WTF', target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'escapes'):
                user_file(root / 'settings', 'WTF/Config.wtf')
            (root / 'cache').symlink_to(root / 'game', target_is_directory=True)
            with self.assertRaises(ValueError):
                validate_paths(root / 'game/Data', root / 'settings', root / 'cache', root / 'logs')

    def test_strict_environment_preserves_all_detectors(self):
        with patch.dict(os.environ, {'LD_PRELOAD': '/unsafe', 'LSAN_OPTIONS': 'suppressions=hidden',
                                     'ASAN_OPTIONS': 'detect_leaks=0', 'DXVK_CONFIG_FILE': '/unsafe'}):
            environment = runtime_environment(Path('/package'), Path('/cache'), Path('/logs'), True)
        self.assertEqual(environment['LD_LIBRARY_PATH'], str(Path('/package/lib')))
        self.assertEqual(environment['ASAN_OPTIONS'], 'detect_leaks=1:halt_on_error=1')
        self.assertEqual(environment['UBSAN_OPTIONS'], 'halt_on_error=1:print_stacktrace=1')
        for key in ['LD_PRELOAD', 'LSAN_OPTIONS', 'DXVK_CONFIG_FILE']:
            self.assertNotIn(key, environment)

    @unittest.skipIf(os.name == 'nt', 'Linux desktop path/URI execution contract')
    def test_desktop_is_relocatable_and_test_activation_is_separate(self):
        with tempfile.TemporaryDirectory(prefix='WoW $ #? quote\' ü ') as temp:
            root = Path(temp)
            # Execute only an authored script to check desktop argument forwarding.
            (root / 'launch.py').write_text('import json,sys; print(json.dumps(sys.argv[1:]))\n')
            for activate in [False, True]:
                desktop = root / 'Play WoW.desktop'
                desktop.write_text(desktop_entry(True, activate))
                arguments = shlex.split(next(line[5:] for line in desktop.read_text().splitlines() if line.startswith('Exec=')))
                arguments[0] = sys.executable
                for location in [str(desktop), desktop.as_uri()]:
                    result = subprocess.run([location if arg == '%k' else arg for arg in arguments],
                                            text=True, capture_output=True, check=True)
                    expected = ['--desktop'] + (['--test-server-login'] if activate else [])
                    self.assertEqual(json.loads(result.stdout), expected)

    def test_normal_package_has_no_test_server_launchers(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            (output / 'WowNativeClient').write_bytes(b'authored packaging fixture')
            stage_launchers(output, SOURCE, False)
            self.assertNotIn('--test-server-login', (output / 'Play WoW.desktop').read_text())
            self.assertFalse((output / 'Play WoW TEST-SERVER.desktop').exists())
            stage_launchers(output, SOURCE, True)
            self.assertIn('--test-server-login', (output / 'Play WoW TEST-SERVER.desktop').read_text())
            self.assertIn('--desktop --test-server-login "$@"', (output / 'Launch TEST-SERVER.sh').read_text())
            stage_launchers(output, SOURCE, False)
            self.assertFalse((output / 'Launch TEST-SERVER.sh').exists())


if __name__ == '__main__':
    unittest.main()
