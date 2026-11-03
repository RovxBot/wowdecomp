"""Actions execution of the actual single-file application without game data."""
import hashlib
import argparse
import json
import os
from pathlib import Path
import subprocess
import shlex
import tarfile
import tempfile


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Packaged binary qualification runs in Actions.')
    parser = argparse.ArgumentParser()
    parser.add_argument('application', type=Path)
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    application = args.application.resolve()
    with tempfile.TemporaryDirectory(prefix='WoW $ space ü ') as temp:
        root = Path(temp)
        game = root / 'game'
        (game / 'Data/enUS').mkdir(parents=True)
        (game / 'WoW.AppImage').symlink_to(application)
        (game / 'Data/enUS/realmlist.wtf').write_text('SET realmlist "127.0.0.1:3724"\n')
        environment = os.environ.copy()
        environment.update(APPIMAGE_EXTRACT_AND_RUN='1', XDG_CONFIG_HOME=str(root / 'config'),
                           XDG_CACHE_HOME=str(root / 'cache'), XDG_STATE_HOME=str(root / 'state'))
        if args.archive:
            with tarfile.open(args.archive) as archive:
                archive.extractall(game, filter='data')
            scripts = list(game.glob('Launch *.sh'))
            assert len(scripts) == 1 and os.access(scripts[0], os.X_OK)
            command = [str(scripts[0])]
            # Exercise extracted launcher with no APPIMAGE/FUSE assistance and
            # no explicit Data path, from an unrelated working directory.
            environment.pop('APPIMAGE_EXTRACT_AND_RUN', None)
            result = subprocess.run(command + ['--check'], cwd=root, env=environment,
                                    check=True, text=True, capture_output=True)
            assert json.loads(result.stdout)['data'] == str(game / 'Data')
            desktop = game / 'Play WoW TEST-SERVER.desktop'
            line = next(line[5:] for line in desktop.read_text().splitlines() if line.startswith('Exec='))
            desktop_command = [desktop.as_uri() if part == '%k' else part for part in shlex.split(line)]
            environment['APPIMAGE_EXTRACT_AND_RUN'] = '1'
        else:
            command = [str(application), '--data', str(game / 'Data')]
        # --data also makes explicit launch independent of AppImage's resolution
        # of a symbolic link to the executable.
        result = subprocess.run(command + ['--check'],
                                cwd=root, env=environment, check=True, text=True, capture_output=True)
        plan = json.loads(result.stdout)
        metadata = json.loads((application.parent / 'manifest.json').read_text())
        assert plan['data'] == str(game / 'Data') and plan['test_server_login'] == metadata['test_server_login']
        assert not (root / 'config').exists(), '--check wrote settings'
        assert not (game / 'WTF').exists()
        # Query the actual login/Glue translation units. A fixture compiled in
        # the CMake root cannot prove definitions reached child directories.
        app_dir = application.parent / 'AppDir'
        native_environment = os.environ.copy()
        native_environment['LD_LIBRARY_PATH'] = str(app_dir / 'lib') + ':' + str(app_dir / 'runtime')
        for active in [False, True]:
            native_environment['WOWDECOMP_ISOLATED_SERVER_LOGIN'] = '1' if active else '0'
            actual = subprocess.run([str(app_dir / 'WowClient64'), '--native-profile'], env=native_environment,
                                    text=True, capture_output=True)
            if actual.returncode:
                raise RuntimeError('Actual login-module capability probe failed (exit ' + str(actual.returncode) + '):\n' + actual.stdout + actual.stderr)
            compiled = json.loads(actual.stdout)
            assert bool(compiled['login_build']) == metadata['test_server_login'], compiled
            assert bool(compiled['glue_build']) == metadata['test_server_login'], compiled
            assert bool(compiled['login_active']) == (active and metadata['test_server_login']), compiled
            assert bool(compiled['glue_active']) == (active and metadata['test_server_login']), compiled
        test = subprocess.run([str(application), '--data', str(game / 'Data'), '--check', '--test-server-login'],
                              cwd=root, env=environment, text=True, capture_output=True)
        assert (test.returncode == 0) == metadata['test_server_login'], test.stdout + test.stderr
        # Run the actual complete executable as well as --check. Authored Data
        # deliberately lacks all MPQs; startup must fail visibly and write only
        # external state. This is not original-data or rendering qualification.
        environment.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        (game / 'WTF/Account/TEST').mkdir(parents=True)
        original = b'SET gxResolution "1280x720"\nSET readEULA "1"\nSET accountName "private-fixture"\n'
        (game / 'WTF/Config.wtf').write_bytes(original)
        (game / 'WTF/Account/TEST/bindings-cache.wtf').write_text('bind W MOVEFORWARD\n')
        failed = subprocess.run(desktop_command if args.archive else command,
                                cwd=root, env=environment, text=True, capture_output=True, timeout=45)
        assert failed.returncode != 0, failed.stdout + failed.stderr
        reports = list((root / 'state').rglob('session.json'))
        assert len(reports) == 1, failed.stdout + failed.stderr
        report = json.loads(reports[0].read_text())
        assert report['exit_code'] > 0 and report['test_server_login'] == metadata['test_server_login'], report
        log = (reports[0].parent / 'client.log').read_text()
        enabled = int(metadata['test_server_login'])
        assert f'native-login-profile login_build={enabled} glue_build={enabled} login_active={enabled} glue_active={enabled}' in log, log
        assert 'symbol lookup error' not in log, log
        assert 'runtime error:' not in log and 'Sanitizer' not in log, log
        assert 'Failed to open archive ' in log, log
        configs = list((root / 'config').rglob('WTF/Config.wtf'))
        assert len(configs) == 1 and 'SET gxResolution "1280x720"' in configs[0].read_text(), configs
        assert 'readEULA' not in configs[0].read_text() and 'private-fixture' not in configs[0].read_text()
        assert 'SET realmlist "127.0.0.1:3724"' in configs[0].read_text()
        assert (configs[0].parent / 'Account/TEST/bindings-cache.wtf').read_text() == 'bind W MOVEFORWARD\n'
        assert (game / 'WTF/Config.wtf').read_bytes() == original
        assert not (game / 'Cache').exists() and not (game / 'Logs').exists()
        print('full-linux-missing-data-stop=' + str(report['exit_code']) + '\n' + log[-1800:])
        assert hashlib.sha256(application.read_bytes()).hexdigest() in (application.parent / (application.name + '.sha256')).read_text()
        print('full-linux-application=passed: actual AppImage execution, paths, no check writes, labelled test profile defaults, real login modules, hash')


if __name__ == '__main__':
    main()
