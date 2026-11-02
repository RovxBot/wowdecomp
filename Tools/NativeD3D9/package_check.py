"""Actions-only execution of the extracted desktop package, without game data."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile
import tempfile

from package import HOST_LIBRARIES, linked_libraries


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Packaged binary qualification runs in Actions.')
    archive = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='WoW space $ #? quote\' ü-') as temp:
        root = Path(temp)
        game = root / 'Clean WoW'
        game.mkdir()
        data = game / 'Data/enUS'
        data.mkdir(parents=True)
        (data / 'realmlist.wtf').write_text('set realmlist "127.0.0.1:3724"\nSET accountName "never-import"\n')
        original = {p.relative_to(game).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in game.rglob('*') if p.is_file()}
        with tarfile.open(archive) as package:
            package.extractall(game, filter='data')
        directories = [p for p in game.iterdir() if p.name.startswith('WoW-Native-Linux')]
        assert len(directories) == 1
        artifact = directories[0]
        manifest = json.loads((artifact / 'manifest.json').read_text())
        environment = os.environ.copy()
        environment.update(XDG_CONFIG_HOME=str(root / 'config'), XDG_CACHE_HOME=str(root / 'cache'),
                           XDG_STATE_HOME=str(root / 'state'), LD_LIBRARY_PATH=str(artifact / 'lib'))
        for binary in [artifact / 'WowNativeClient', *sorted((artifact / 'lib').glob('*'))]:
            assert binary.is_file()
            for name, path in linked_libraries(binary, environment).items():
                assert name in HOST_LIBRARIES or path.resolve().is_relative_to(artifact / 'lib'), (name, str(path))
        shell = artifact / 'Launch WoW.sh'
        assert os.access(shell, os.X_OK)
        desktop = artifact / 'Play WoW.desktop'
        command = shlex.split(next(line[5:] for line in desktop.read_text().splitlines() if line.startswith('Exec=')))
        command = [str(desktop) if arg == '%k' else arg for arg in command]
        # Test both absolute file paths and file:// URIs received through %k.
        for location in [str(desktop), desktop.as_uri()]:
            checked = subprocess.run([location if arg == str(desktop) else arg for arg in command] + ['--check'],
                                     cwd=root, env=environment, text=True, capture_output=True, check=True)
            plan = json.loads(checked.stdout)
            assert plan['data'] == str(data.parent)
            assert plan['test_server_login'] is False
        test = artifact / 'Play WoW TEST-SERVER.desktop'
        assert test.exists() == manifest['test_server_login']
        if test.exists():
            test_command = shlex.split(next(line[5:] for line in test.read_text().splitlines() if line.startswith('Exec=')))
            test_command = [str(test) if arg == '%k' else arg for arg in test_command]
            result = subprocess.run(test_command + ['--check'], env=environment, cwd=root, text=True, capture_output=True, check=True)
            assert json.loads(result.stdout)['test_server_login'] is True
        else:
            refused = subprocess.run(command + ['--test-server-login', '--check'], env=environment, cwd=root, text=True, capture_output=True)
            assert refused.returncode == 1 and 'TEST-SERVER DXVK artifact' in refused.stderr
        # The real binary must load the bundled runtime, reach archive validation,
        # fail on deliberately absent MPQs and retain its failed session externally.
        failed = subprocess.run([str(shell)], cwd=root, env=environment, text=True, capture_output=True)
        assert failed.returncode == 1, failed.stdout + failed.stderr
        reports = list((root / 'state').rglob('session.json'))
        assert len(reports) == 1
        report = json.loads(reports[0].read_text())
        assert report['exit_code'] == 1 and report['test_server_login'] is False
        log = (reports[0].parent / 'client.log').read_text()
        assert 'Original archive authentication:' in log and 'workers=0' in log, log
        settings = Path(report['settings']) / 'WTF/Config.wtf'
        assert settings.read_text().strip() == 'SET realmlist "127.0.0.1:3724"'
        assert original == {p: hashlib.sha256((game / p).read_bytes()).hexdigest() for p in original}
        assert not any((game / name).exists() for name in ['WTF', 'Cache', 'Logs'])
        # Relocate the package: identity follows Data, never the extraction folder.
        relocated = game / 'another package folder'
        artifact.rename(relocated)
        checked = subprocess.run([str(relocated / 'Launch WoW.sh'), '--check'], cwd=root,
                                 env=environment, text=True, capture_output=True, check=True)
        assert json.loads(checked.stdout)['settings'] == report['settings']
        print('desktop-package-smoke=passed: relocated archive, runtime closure, desktop/URI and shell launch, '
              'explicit profile gates, external settings/logs, unchanged authored data; expected missing-MPQ stop')


if __name__ == '__main__':
    main()
