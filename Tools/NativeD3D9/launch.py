#!/usr/bin/env python3
"""Launch the packaged Linux client with read-only original data and external state."""
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

# The package may live inside a read-only original installation. Imports must
# not create __pycache__ beside the launcher, including direct diagnostic use.
sys.dont_write_bytecode = True
from run_native_client import dxvk_library_path, require_test_server_artifact, validate_paths, verify_artifact


def child(directory, name):
    """Match the native filesystem's ASCII case rules, including ambiguity refusal."""
    matches = [p for p in directory.iterdir() if p.name.lower() == name.lower()]
    if len(matches) > 1:
        raise ValueError('Ambiguous original data path: ' + str(directory / name))
    return matches[0] if matches else None


def discover_data(package, explicit=None):
    if explicit is not None:
        data = explicit.resolve()
    else:
        # Extract directly beside Data or keep the package in one adjacent folder.
        data = child(package, 'Data') or child(package.parent, 'Data')
        if data is None:
            raise ValueError('Cannot find Data. Extract this package into your clean WoW 3.3.5a folder '
                             '(beside Data), or use --data /path/to/WoW/Data.')
        data = data.resolve()
    if not data.is_dir():
        raise ValueError('The original Data directory is missing: ' + str(data))
    # The integrated client currently authenticates enUS, regardless of saved locale.
    locale = child(data, 'enUS')
    if locale is None or not locale.is_dir():
        raise ValueError('This client requires clean build 12340 enUS data (Data/enUS).')
    return data


def xdg_root(environment, name, fallback):
    value = Path(environment.get(name) or fallback)
    if not value.is_absolute():
        raise ValueError(name + ' must be an absolute path')
    return value


def user_paths(data, manifest, environment):
    identity = hashlib.sha256(os.fsencode(data)).hexdigest()[:16]
    profile = 'test-server' if manifest.get('test_server_login') is True else 'release'
    if manifest['preset'].endswith('-asan'):
        profile += '-strict'
    suffix = Path('wowdecomp') / profile / identity
    home = str(Path.home())
    return tuple(xdg_root(environment, key, home + fallback) / suffix for key, fallback in [
        ('XDG_CONFIG_HOME', '/.config'), ('XDG_CACHE_HOME', '/.cache'), ('XDG_STATE_HOME', '/.local/state')])


def user_file(root, relative):
    path = root / relative
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError('Writable path escapes its external root: ' + str(path))
    return path


def read_endpoint(path):
    if path.stat().st_size > 1024 * 1024:
        raise ValueError('Realmlist/config file exceeds 1 MiB: ' + str(path))
    endpoint = None
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        if not re.match(r'^\s*set\s+realmlist(?:\s|$)', line, re.IGNORECASE):
            continue
        match = re.fullmatch(r'\s*set\s+realmlist\s+(?:"([^"\r\n]*)"|([^\s";]+))\s*(?:(?://|#).*)?', line, re.IGNORECASE)
        if not match:
            raise ValueError('Invalid SET realmlist in ' + str(path))
        endpoint = match[1] if match[1] is not None else match[2]
    return endpoint


def seed_realmlist(data, settings):
    """Copy only the endpoint once; never import accounts, agreements or other CVars."""
    config = user_file(settings, 'WTF/Config.wtf')
    if config.exists() and read_endpoint(config) is not None:
        return
    locale = child(data, 'enUS')
    source = child(locale, 'realmlist.wtf') or child(data.parent, 'realmlist.wtf')
    if source is None:
        return  # Normal startup remains usable; test login reports a missing endpoint.
    endpoint = read_endpoint(source)
    if endpoint is None:
        return
    if not re.fullmatch(r'[A-Za-z0-9.-]+(?::[0-9]{1,5})?', endpoint) or len(endpoint) > 255:
        raise ValueError('Expected realmlist host[:port] in ' + str(source))
    if ':' in endpoint and not 1 <= int(endpoint.rsplit(':', 1)[1]) <= 65535:
        raise ValueError('Invalid realmlist port in ' + str(source))
    config.parent.mkdir(parents=True, exist_ok=True)
    with config.open('a', encoding='utf-8') as stream:
        stream.write('\nSET realmlist "' + endpoint + '"\n')


def runtime_environment(package, cache, logs, strict):
    environment = os.environ.copy()
    for key in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS', 'DXVK_CONFIG', 'DXVK_CONFIG_FILE',
                'DXVK_FILTER_DEVICE_NAME', 'DXVK_FILTER_DEVICE_UUID']:
        environment.pop(key, None)
    environment.update(LD_LIBRARY_PATH=dxvk_library_path(package), SDL_VIDEODRIVER='x11', DXVK_WSI_DRIVER='SDL3',
                       DXVK_LOG_PATH=str(logs), XDG_CACHE_HOME=str(cache),
                       DXVK_STATE_CACHE_PATH=str(cache), MESA_SHADER_CACHE_DIR=str(cache / 'mesa'))
    if strict:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    return environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, default=Path(__file__).resolve().parent)
    for key in ['data', 'settings', 'cache', 'logs']:
        parser.add_argument('--' + key, type=Path)
    parser.add_argument('--test-server-login', action='store_true', help='Explicit TEST-SERVER runtime opt-in; isolated character-selection testing')
    parser.add_argument('--check', action='store_true', help='Verify package and show resolved paths without starting or writing settings')
    parser.add_argument('--desktop', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    os.umask(0o077)
    try:
        if platform.system() != 'Linux' or platform.machine() != 'x86_64':
            raise ValueError('This package requires Linux x86-64.')
        package = args.artifact.resolve()
        metadata = json.loads((package / 'manifest.json').read_text())
        strict = metadata.get('preset') == 'linux-client-d3d9-asan'
        manifest, binary = verify_artifact(package, strict, 'dxvk')
        if args.test_server_login:
            require_test_server_artifact(manifest, 'dxvk')
        data = discover_data(package, args.data)
        settings, cache, logs = user_paths(data, manifest, os.environ)
        settings, cache, logs = ((override or default).resolve() for override, default in [
            (args.settings, settings), (args.cache, cache), (args.logs, logs)])
        validate_paths(data, settings, cache, logs)
        for path in [settings, cache, logs]:
            if path.is_relative_to(package) or package.is_relative_to(path):
                raise ValueError('Writable state must be outside the verified package')
        user_file(settings, 'WTF/Config.wtf')
        plan = dict(data=str(data), settings=str(settings), cache=str(cache), logs=str(logs),
                    source_commit=manifest['source_commit'], test_server_login=args.test_server_login,
                    test_server_build=manifest.get('test_server_login', False), strict=strict)
        if args.check:
            print(json.dumps(plan, indent=2))
            return 0
        for path in [settings, cache, logs]:
            path.mkdir(parents=True, exist_ok=True)
        seed_realmlist(data, settings)
        session = Path(tempfile.mkdtemp(prefix=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ-'), dir=logs))
        print('Starting WoW Native' + (' — TEST-SERVER character-selection testing' if args.test_server_login else ''))
        print('Settings: ' + str(settings) + '\nSession logs: ' + str(session), flush=True)
        environment = runtime_environment(package, cache, session, strict)
        command = [str(binary), '--renderer', 'dxvk', '--data', str(data), '--settings', str(settings), '--cache', str(cache)]
        if args.test_server_login:
            command.append('--test-server-login')
        # No frame cap, screenshot, or diagnostic timeout during an interactive session.
        with (session / 'client.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=settings, env=environment, stdout=log, stderr=subprocess.STDOUT)
        plan.update(exit_code=result.returncode, workflow_run=manifest['workflow_run'])
        (session / 'session.json').write_text(json.dumps(plan, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError('The client stopped (exit ' + str(result.returncode) + ').\nSee ' + str(session / 'client.log'))
        return 0
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print('WoW Native could not start or complete the session:\n' + str(error), file=sys.stderr, flush=True)
        if args.desktop and sys.stdin.isatty():
            try:
                input('Press Enter to close this window. ')
            except EOFError:
                pass
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
