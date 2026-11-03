#!/usr/bin/env python3
"""Launch the full Linux client from one AppImage with read-only original data."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def child(root, name):
    found = [p for p in root.iterdir() if p.name.lower() == name.lower()]
    if len(found) > 1:
        raise ValueError('Ambiguous original path: ' + str(root / name))
    return found[0] if found else None


def data_path(application, explicit):
    data = explicit if explicit else child(application.parent, 'Data')
    if data is None:
        raise ValueError('Place this application beside your clean WoW Data folder, or use --data /path/to/WoW/Data.')
    data = data.resolve()
    if not data.is_dir() or child(data, 'enUS') is None:
        raise ValueError('Clean 3.3.5a/12340 enUS Data is required.')
    return data


def paths(data, metadata, environment):
    identity = hashlib.sha256(os.fsencode(data)).hexdigest()[:16]
    profile = ('test-server' if metadata['test_server_login'] else 'normal') + ('-strict' if metadata['strict'] else '')
    suffix = Path('wowdecomp/full-linux') / profile / identity
    result = []
    for name, fallback in [('XDG_CONFIG_HOME', '.config'), ('XDG_CACHE_HOME', '.cache'), ('XDG_STATE_HOME', '.local/state')]:
        base = Path(environment.get(name) or Path.home() / fallback)
        if not base.is_absolute():
            raise ValueError(name + ' must be absolute')
        result.append((base / suffix).resolve())
    return result


def validate(data, package, roots):
    game = data.parent
    for i, root in enumerate(roots):
        for protected in [game, package]:
            if root.is_relative_to(protected) or protected.is_relative_to(root):
                raise ValueError('Writable state overlaps the game or application: ' + str(root))
        for other in roots[:i]:
            if root.is_relative_to(other) or other.is_relative_to(root):
                raise ValueError('Settings, cache and logs must be separate directories')


def link(path, target):
    if path.is_symlink():
        if path.resolve() != target.resolve():
            raise ValueError('Unexpected state link: ' + str(path))
    elif path.exists():
        raise ValueError('Expected a managed external-state link: ' + str(path))
    else:
        path.symlink_to(target, target_is_directory=True)


def seed_endpoint(data, settings):
    config = settings / 'WTF/Config.wtf'
    if config.exists():
        return  # Never change existing user CVars or agreement/account decisions.
    locale = child(data, 'enUS')
    source = child(locale, 'realmlist.wtf')
    if source is None:
        return
    if source.stat().st_size > 1024 * 1024:
        raise ValueError('realmlist.wtf exceeds 1 MiB')
    endpoint = None
    for line in source.read_text(encoding='utf-8-sig').splitlines():
        match = re.fullmatch(r'\s*set\s+realmlist\s+"?([A-Za-z0-9.-]+(?::[0-9]{1,5})?)"?\s*', line, re.I)
        if match:
            endpoint = match[1]
    if endpoint is not None:
        if ':' in endpoint and not 1 <= int(endpoint.rsplit(':', 1)[1]) <= 65535:
            raise ValueError('Invalid realmlist port')
        config.write_text('SET realmlist "' + endpoint + '"\nSET locale "enUS"\n')


def prepare_state(data, settings, cache, logs):
    for root in [settings, cache, logs]:
        root.mkdir(parents=True, exist_ok=True)
    # Refuse old/malicious writable links before the client can follow them.
    for root in [settings, cache, logs]:
        for path in root.rglob('*'):
            if path.is_symlink():
                allowed = {settings / 'Data': data, settings / 'Cache': cache, settings / 'Logs': logs}
                addons = child(data.parent, 'Interface')
                if addons is not None:
                    allowed[settings / 'Interface/AddOns'] = addons / 'AddOns'
                if path not in allowed or path.resolve() != allowed[path].resolve():
                    raise ValueError('Unexpected writable-state symlink: ' + str(path))
    link(settings / 'Data', data)
    link(settings / 'Cache', cache)
    link(settings / 'Logs', logs)
    for name in ['WTF', 'Interface']:
        (settings / name).mkdir(exist_ok=True)
    interface = child(data.parent, 'Interface')
    if interface is not None:
        addons = child(interface, 'AddOns')
        if addons is not None:
            link(settings / 'Interface/AddOns', addons)
    seed_endpoint(data, settings)


def verify(package):
    metadata = json.loads((package / 'manifest.json').read_text())
    for relative, expected in metadata['files'].items():
        path = package / relative
        if not path.resolve().is_relative_to(package) or not path.is_file() or digest(path) != expected:
            raise ValueError('Application file verification failed: ' + relative)
    for directory in ['lib', 'runtime']:
        actual = {p.relative_to(package).as_posix() for p in (package / directory).glob('*')}
        expected = {p for p in metadata['files'] if p.startswith(directory + '/')}
        if actual != expected:
            raise ValueError('Unrecorded application library in ' + directory)
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path)
    parser.add_argument('--check', action='store_true', help='Verify the application and print paths without starting the client')
    parser.add_argument('--test-server-login', action='store_true', help='Explicit isolated-server test activation; server must already allow native integrity-unavailable sessions')
    args = parser.parse_args()
    os.umask(0o077)
    try:
        if platform.system() != 'Linux' or platform.machine() != 'x86_64':
            raise ValueError('Linux x86-64 is required')
        package = Path(__file__).resolve().parent
        metadata = verify(package)
        application = Path(os.environ.get('APPIMAGE', package / 'WoW-Linux.AppImage')).resolve()
        data = data_path(application, args.data)
        settings, cache, logs = paths(data, metadata, os.environ)
        validate(data, package, [settings, cache, logs])
        if args.test_server_login and not metadata['test_server_login']:
            raise ValueError('This is a normal build; use the separately labelled TEST-SERVER application')
        plan = dict(source_commit=metadata['source_commit'], upstream_commit=metadata['upstream_commit'],
                    data=str(data), settings=str(settings), cache=str(cache), logs=str(logs),
                    test_server_login=args.test_server_login, strict=metadata['strict'])
        if args.check:
            print(json.dumps(plan, indent=2))
            return 0
        prepare_state(data, settings, cache, logs)
        session = Path(tempfile.mkdtemp(prefix=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ-'), dir=logs))
        environment = os.environ.copy()
        for key in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS', 'WOWDECOMP_AUTOLOGIN', 'WOWDECOMP_ISOLATED_SERVER_LOGIN',
                    'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'PYTHONPATH', 'PYTHONHOME']:
            environment.pop(key, None)
        # Host distribution libraries precede bundled fallbacks, preserving the
        # host ICD's ABI requirements. SDL is the private Actions-built runtime.
        environment['LD_LIBRARY_PATH'] = ':'.join(map(str, [package / 'lib', Path('/usr/lib/x86_64-linux-gnu'),
                                                             Path('/lib/x86_64-linux-gnu'), Path('/usr/lib64'),
                                                             Path('/usr/lib'), Path('/lib64'), Path('/lib'), package / 'runtime']))
        environment.update(WOWDECOMP_GAME_ROOT=str(data.parent), WOWDECOMP_SETTINGS=str(settings),
                           WOWDECOMP_CACHE=str(cache), WOWDECOMP_LOGS=str(logs), XDG_CONFIG_HOME=str(settings),
                           XDG_CACHE_HOME=str(cache), XDG_STATE_HOME=str(logs),
                           MESA_SHADER_CACHE_DIR=str(cache / 'mesa'))
        if args.test_server_login:
            environment['WOWDECOMP_ISOLATED_SERVER_LOGIN'] = '1'
            print('TEST-SERVER: executable integrity, ScanDLL and Warden are unavailable. '
                  'The server must already have StrictVersionCheck=0 and Warden.Enabled=0.', flush=True)
        if metadata['strict']:
            environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        print('Starting WoW. Session log: ' + str(session / 'client.log'), flush=True)
        with (session / 'client.log').open('w') as log:
            result = subprocess.run([str(package / 'WowClient64')], cwd=settings, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT)
        plan.update(exit_code=result.returncode, workflow_run=metadata['workflow_run'])
        (session / 'session.json').write_text(json.dumps(plan, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError('Client stopped with exit ' + str(result.returncode) + '. See ' + str(session / 'client.log'))
        return 0
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print('WoW could not complete the session: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
