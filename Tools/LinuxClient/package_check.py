"""Actions execution of the actual single-file application without game data."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Packaged binary qualification runs in Actions.')
    application = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='WoW $ space ü ') as temp:
        root = Path(temp)
        game = root / 'game'
        (game / 'Data/enUS').mkdir(parents=True)
        (game / 'WoW.AppImage').symlink_to(application)
        (game / 'Data/enUS/realmlist.wtf').write_text('SET realmlist "127.0.0.1:3724"\n')
        environment = os.environ.copy()
        environment.update(APPIMAGE_EXTRACT_AND_RUN='1', XDG_CONFIG_HOME=str(root / 'config'),
                           XDG_CACHE_HOME=str(root / 'cache'), XDG_STATE_HOME=str(root / 'state'))
        # --data also makes explicit launch independent of AppImage's resolution
        # of a symbolic link to the executable.
        result = subprocess.run([str(application), '--data', str(game / 'Data'), '--check'],
                                cwd=root, env=environment, check=True, text=True, capture_output=True)
        plan = json.loads(result.stdout)
        assert plan['data'] == str(game / 'Data') and not plan['test_server_login']
        assert not (root / 'config').exists(), '--check wrote settings'
        assert not (game / 'WTF').exists()
        metadata = json.loads((application.parent / 'manifest.json').read_text())
        test = subprocess.run([str(application), '--data', str(game / 'Data'), '--check', '--test-server-login'],
                              cwd=root, env=environment, text=True, capture_output=True)
        assert (test.returncode == 0) == metadata['test_server_login'], test.stdout + test.stderr
        # Run the actual complete executable as well as --check. Authored Data
        # deliberately lacks all MPQs; startup must fail visibly and write only
        # external state. This is not original-data or rendering qualification.
        environment.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        failed = subprocess.run([str(application), '--data', str(game / 'Data')],
                                cwd=root, env=environment, text=True, capture_output=True, timeout=45)
        assert failed.returncode != 0, failed.stdout + failed.stderr
        reports = list((root / 'state').rglob('session.json'))
        assert len(reports) == 1, failed.stdout + failed.stderr
        report = json.loads(reports[0].read_text())
        assert report['exit_code'] != 0 and not report['test_server_login']
        log = (reports[0].parent / 'client.log').read_text()
        assert 'symbol lookup error' not in log, log
        assert 'runtime error:' not in log and 'ERROR: AddressSanitizer' not in log, log
        assert not (game / 'WTF').exists() and not (game / 'Cache').exists() and not (game / 'Logs').exists()
        print('full-linux-missing-data-stop=' + str(report['exit_code']) + '\n' + log[-1800:])
        assert hashlib.sha256(application.read_bytes()).hexdigest() in (application.parent / (application.name + '.sha256')).read_text()
        print('full-linux-application=passed: actual AppImage extraction/execution, paths, no check writes, explicit test profile, hash')


if __name__ == '__main__':
    main()
