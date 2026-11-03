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
import shutil
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
    existing = config.read_bytes() if config.exists() else b''
    if re.search(rb'(?im)^\s*set\s+realmlist\s+', existing):
        return
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
        with config.open('ab') as output:
            output.write((b'\n' if existing and not existing.endswith(b'\n') else b'') +
                         ('SET realmlist "' + endpoint + '"\n').encode())
            if not re.search(rb'(?im)^\s*set\s+locale\s+', existing):
                output.write(b'SET locale "enUS"\n')


# Preferences are local user data. Never transfer login secrets, agreement or
# scanner approval, or a Windows renderer selection into a native profile.
EXCLUDED_CVARS = {b'accountname', b'accountlist', b'password', b'username',
                  b'readeula', b'readtos', b'readtermination', b'readscanning',
                  b'acceptedtos', b'scandllokaytologin', b'gxapi'}


def preference_lines(contents):
    result = []
    for line in contents.splitlines(keepends=True):
        match = re.match(rb'\s*SET\s+([A-Za-z0-9_]+)\s+', line, re.I)
        if match and match[1].lower() not in EXCLUDED_CVARS:
            result.append((match[1].lower(), line.rstrip(b'\r\n') + b'\n'))
    return result


def import_settings(data, settings):
    """Snapshot original WTF once; all later saves belong to the native profile."""
    marker = settings / 'wtf-import.json'
    if marker.exists():
        return False
    original = child(data.parent, 'WTF')
    copied = 0
    if original is not None:
        # Do not follow source links or accept case-colliding paths on Linux.
        # Validate the complete snapshot before making any changes.
        entries = []
        def collect(directory, relative=Path()):
            names = set()
            for source in sorted(directory.iterdir()):
                key = source.name.lower()
                if key in names:
                    raise ValueError('Ambiguous original WTF path: ' + str(directory))
                names.add(key)
                if source.is_symlink():
                    raise ValueError('Original WTF symlinks cannot be imported: ' + str(source))
                name = 'Config.wtf' if not relative.parts and key == 'config.wtf' else source.name
                if not relative.parts and key == 'account':
                    name = 'Account'
                target = relative / name
                if source.is_dir():
                    collect(source, target)
                elif source.is_file() and (target == Path('Config.wtf') or
                        (target.parts[0] == 'Account' and key.endswith(('.wtf', '.lua', '.lua.bak', '.txt')))):
                    if source.stat().st_size > 64 * 1024 * 1024:
                        raise ValueError('Original WTF file exceeds 64 MiB: ' + str(source))
                    entries.append((source, settings / 'WTF' / target))
        if original.is_symlink() or not original.is_dir():
            raise ValueError('Original WTF must be a directory without symlinks')
        collect(original)
        for source, target in entries:
            target.parent.mkdir(parents=True, exist_ok=True)
            if target == settings / 'WTF/Config.wtf':
                imported = preference_lines(source.read_bytes().removeprefix(b'\xef\xbb\xbf'))
                current = target.read_bytes() if target.exists() else b''
                keys = {key for key, line in imported}
                # Preserve native agreement decisions and other native-only CVars.
                retained = [line for line in current.splitlines(keepends=True)
                            if not (match := re.match(rb'\s*SET\s+([A-Za-z0-9_]+)\s+', line, re.I))
                            or match[1].lower() not in keys]
                backup = target.with_name('Config.before-import.wtf')
                if current and not backup.exists():
                    backup.write_bytes(current)
                target.write_bytes(b''.join(retained) + b'\n' + b''.join(line for key, line in imported))
                copied += 1
            elif not target.exists():
                contents = source.read_bytes()
                if source.suffix.lower() == '.wtf':
                    contents = b''.join(line for line in contents.splitlines(keepends=True)
                                        if not (match := re.match(rb'\s*SET\s+([A-Za-z0-9_]+)\s+', line, re.I))
                                        or match[1].lower() not in EXCLUDED_CVARS)
                target.write_bytes(contents)
                copied += 1
    marker.write_text(json.dumps(dict(version=1, files_copied=copied)) + '\n')
    return True


def configured_endpoint(settings):
    config = settings / 'WTF/Config.wtf'
    if not config.is_file() or config.stat().st_size > 1024 * 1024:
        raise ValueError('TEST-SERVER requires an explicit external SET realmlist host[:port]')
    endpoint = None
    for line in config.read_text(encoding='utf-8-sig').splitlines():
        match = re.fullmatch(r'\s*set\s+realmlist\s+"?([A-Za-z0-9.-]+(?::[0-9]{1,5})?)"?\s*', line, re.I)
        if match:
            endpoint = match[1]
    if not endpoint:
        raise ValueError('TEST-SERVER requires an explicit external SET realmlist host[:port]')
    return endpoint


def prepare_state(data, settings, cache, logs):
    for root in [settings, cache, logs]:
        root.mkdir(parents=True, exist_ok=True)
    # Refuse old/malicious writable links before the client can follow them.
    for root in [settings, cache, logs]:
        for path in root.rglob('*'):
            if path.is_symlink():
                # Fontconfig and other native libraries keep versioned cache
                # aliases. A link confined to the same external root is safe.
                if path.resolve().is_relative_to(root):
                    continue
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
    if import_settings(data, settings) or not (settings / 'WTF/Config.wtf').exists():
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


def runtime_path(package, session, environment):
    """Expose only missing distribution libraries, preserving host RUNPATHs.

    A flat directory of every fallback overrides private dependencies such as
    libpulsecommon in a host libpulse RUNPATH. Resolve the actual host closure
    first, adding a verified fallback only for an unresolved SONAME.
    """
    fallback = session / 'runtime'
    fallback.mkdir()
    environment = environment.copy()
    environment['LD_LIBRARY_PATH'] = os.pathsep.join(map(str, [package / 'lib', fallback]))
    binaries = [package / 'WowClient64', *sorted((package / 'lib').glob('*'))]
    loader = subprocess.run(['ldconfig', '-p'], text=True, capture_output=True, check=True).stdout
    if not re.search(r'\blibvulkan\.so\.1 \(.*x86-64', loader):
        shutil.copy2(package / 'runtime/libvulkan.so.1', fallback / 'libvulkan.so.1')
        binaries.append(fallback / 'libvulkan.so.1')
    for unused in range(len(list((package / 'runtime').glob('*'))) + 1):
        missing = set()
        for binary in binaries:
            result = subprocess.run(['ldd', str(binary)], env=environment, text=True, capture_output=True)
            if result.returncode:
                raise ValueError('Cannot resolve native runtime: ' + result.stdout + result.stderr)
            missing.update(re.findall(r'^\s*(\S+) => not found$', result.stdout, re.M))
        if not missing:
            return environment['LD_LIBRARY_PATH']
        for name in missing:
            source = package / 'runtime' / name
            if not source.is_file() or '/' in name or (fallback / name).exists():
                raise ValueError('Unresolved host dependency: ' + name)
            shutil.copy2(source, fallback / name)
            binaries.append(fallback / name)
    raise ValueError('Native dependency closure did not converge')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path)
    parser.add_argument('--check', action='store_true', help='Verify the application and print paths without starting the client')
    parser.add_argument('--test-server-login', action='store_true', help='Compatibility alias; the labelled TEST-SERVER application already enables interactive sign-in')
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
        login_enabled = bool(metadata['test_server_login'])
        plan = dict(source_commit=metadata['source_commit'], upstream_commit=metadata['upstream_commit'],
                    data=str(data), settings=str(settings), cache=str(cache), logs=str(logs),
                    test_server_login=login_enabled, strict=metadata['strict'])
        if args.check:
            print(json.dumps(plan, indent=2))
            return 0
        prepare_state(data, settings, cache, logs)
        session = Path(tempfile.mkdtemp(prefix=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ-'), dir=logs))
        environment = os.environ.copy()
        for key in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS', 'WOWDECOMP_AUTOLOGIN', 'WOWDECOMP_ISOLATED_SERVER_LOGIN', 'WOWDECOMP_ISOLATED_ENDPOINT',
                    'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'PYTHONPATH', 'PYTHONHOME']:
            environment.pop(key, None)
        environment['LD_LIBRARY_PATH'] = runtime_path(package, session, environment)
        environment.update(WOWDECOMP_GAME_ROOT=str(data.parent), WOWDECOMP_SETTINGS=str(settings),
                           WOWDECOMP_CACHE=str(cache), WOWDECOMP_LOGS=str(logs), XDG_CONFIG_HOME=str(settings),
                           XDG_CACHE_HOME=str(cache), XDG_STATE_HOME=str(logs),
                           MESA_SHADER_CACHE_DIR=str(cache / 'mesa'))
        if login_enabled:
            environment['WOWDECOMP_ISOLATED_SERVER_LOGIN'] = '1'
            environment['WOWDECOMP_ISOLATED_ENDPOINT'] = configured_endpoint(settings)
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
