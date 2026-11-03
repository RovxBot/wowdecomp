import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

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

    def test_profile_identity_and_absolute_xdg(self):
        normal = dict(test_server_login=False, strict=False)
        test = dict(test_server_login=True, strict=False)
        env = dict(XDG_CONFIG_HOME=str(self.root / 'config'), XDG_CACHE_HOME=str(self.root / 'cache'), XDG_STATE_HOME=str(self.root / 'logs'))
        self.assertNotEqual(launcher.paths(self.data, normal, env), launcher.paths(self.data, test, env))
        with self.assertRaises(ValueError):
            launcher.paths(self.data, normal, dict(XDG_CONFIG_HOME='relative'))


if __name__ == '__main__':
    unittest.main()
