import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('full_linux_launcher', Path(__file__).with_name('launch.py'))
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='wow $ space ü ')
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        self.data = self.game / 'Data'
        (self.data / 'enUS').mkdir(parents=True)
        self.settings, self.cache, self.logs = [self.root / p for p in ['settings', 'cache', 'logs']]

    def tearDown(self):
        self.temp.cleanup()

    def test_adjacent_data_independent_of_working_directory(self):
        self.assertEqual(launcher.data_path(self.game / 'WoW.AppImage', None), self.data)
        (self.game / 'data').mkdir()
        with self.assertRaises(ValueError):
            launcher.data_path(self.game / 'WoW.AppImage', None)

    def test_only_endpoint_is_seeded_and_decisions_are_preserved(self):
        original = 'SET realmlist "127.0.0.1:3724"\nSET accountName "do-not-import"\nSET readEULA "1"\n'
        realmlist = self.data / 'enUS/realmlist.wtf'
        realmlist.write_text(original)
        launcher.prepare_state(self.data, self.settings, self.cache, self.logs)
        config = self.settings / 'WTF/Config.wtf'
        self.assertEqual(config.read_text(), 'SET realmlist "127.0.0.1:3724"\nSET locale "enUS"\n')
        config.write_text('user decisions\n')
        launcher.prepare_state(self.data, self.settings, self.cache, self.logs)
        self.assertEqual(config.read_text(), 'user decisions\n')
        self.assertEqual(realmlist.read_text(), original)
        self.assertEqual((self.settings / 'Data').resolve(), self.data)
        self.assertFalse((self.game / 'WTF').exists())

    def test_writable_symlink_escape_and_overlaps_are_refused(self):
        self.settings.mkdir()
        (self.settings / 'WTF').symlink_to(self.game, target_is_directory=True)
        with self.assertRaises(ValueError):
            launcher.prepare_state(self.data, self.settings, self.cache, self.logs)
        with self.assertRaises(ValueError):
            launcher.validate(self.data, self.root / 'package', [self.game / 'state', self.cache, self.logs])
        with self.assertRaises(ValueError):
            launcher.validate(self.data, self.root / 'package', [self.settings, self.settings / 'cache', self.logs])

    def test_repeat_launch_accepts_internal_fontconfig_alias(self):
        launcher.prepare_state(self.data, self.settings, self.cache, self.logs)
        directory = self.cache / 'fontconfig'
        directory.mkdir()
        (directory / 'cache-12').write_text('native library cache')
        (directory / 'cache-9').symlink_to('cache-12')
        launcher.prepare_state(self.data, self.settings, self.cache, self.logs)
        self.assertEqual((directory / 'cache-9').read_text(), 'native library cache')

    def test_profile_identity_and_absolute_xdg(self):
        normal = dict(test_server_login=False, strict=False)
        test = dict(test_server_login=True, strict=False)
        env = dict(XDG_CONFIG_HOME=str(self.root / 'config'), XDG_CACHE_HOME=str(self.root / 'cache'), XDG_STATE_HOME=str(self.root / 'logs'))
        self.assertNotEqual(launcher.paths(self.data, normal, env), launcher.paths(self.data, test, env))
        with self.assertRaises(ValueError):
            launcher.paths(self.data, normal, dict(XDG_CONFIG_HOME='relative'))

    def test_runtime_does_not_mask_private_host_dependencies(self):
        package, session = self.root / 'package', self.root / 'session'
        (package / 'lib').mkdir(parents=True)
        (package / 'runtime').mkdir()
        session.mkdir()
        (package / 'runtime/libmissing.so.1').write_text('fixture')
        (package / 'runtime/libpulsecommon-17.0.so').write_text('must not shadow host')
        calls = []
        def run(command, **kwargs):
            calls.append(command)
            if command[0] == 'ldconfig':
                return SimpleNamespace(returncode=0, stdout='libvulkan.so.1 (libc6,x86-64) => /host/libvulkan.so.1', stderr='')
            missing = not (session / 'runtime/libmissing.so.1').exists()
            return SimpleNamespace(returncode=0, stdout='libmissing.so.1 => not found\n' if missing else 'libpulsecommon-17.0.so => /host/private/libpulsecommon-17.0.so\n', stderr='')
        with patch.object(launcher.subprocess, 'run', side_effect=run):
            result = launcher.runtime_path(package, session, {})
        self.assertEqual(result, str(package / 'lib') + os.pathsep + str(session / 'runtime'))
        self.assertEqual((session / 'runtime/libmissing.so.1').read_bytes(), (package / 'runtime/libmissing.so.1').read_bytes())
        self.assertFalse((session / 'runtime/libmissing.so.1').is_symlink())
        self.assertFalse((session / 'runtime/libpulsecommon-17.0.so').exists())


if __name__ == '__main__':
    unittest.main()
