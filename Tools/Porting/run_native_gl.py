"""Run a hash-verified Actions OpenGL probe; never compile locally."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--cycles', type=int, default=1)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--sanitizers', action='store_true')
    for name in ['data', 'settings', 'cache']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--locale', default='enUS')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('This probe currently targets Linux x86-64')
    if not 1 <= args.frames <= 36000 or not 1 <= args.cycles <= 20 or args.timeout < 1:
        parser.error('Invalid frame/cycle count or timeout')
    if any([args.data, args.settings, args.cache]) and not all([args.data, args.settings, args.cache]):
        parser.error('Use --data, --settings and --cache together')
    if args.data:
        # Treat the entire installation as read-only, including captures/reports.
        installation = args.data.resolve().parent
        for output in [args.output, args.snapshot, args.settings, args.cache]:
            if output and output.resolve().is_relative_to(installation):
                parser.error('Outputs and user roots must be outside the game installation')
    manifest = json.loads((args.artifact / 'manifest.json').read_text())
    if manifest['preset'] not in {'linux-gl', 'linux-gl-asan'} or manifest['target_os'] != 'LINUX':
        parser.error('Expected a Linux M3 OpenGL Actions artifact')
    binary = args.artifact / 'bin/NativeGlCheck'
    actual = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = next(item['sha256'] for item in manifest['binaries'] if item['name'] == 'NativeGlCheck')
    if actual != expected:
        raise SystemExit('Hosted OpenGL binary hash does not match the manifest')
    binary.chmod(0o755)
    environment = os.environ.copy()
    environment.update(SDL_VIDEODRIVER='x11', EGL_PLATFORM='x11')
    if args.sanitizers:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    command = [str(binary.resolve()), '--frames', str(args.frames), '--cycles', str(args.cycles)]
    if args.snapshot:
        command += ['--snapshot', str(args.snapshot.resolve())]
    if args.data:
        command += ['--data', str(args.data.resolve()), '--settings', str(args.settings.resolve()),
                    '--cache', str(args.cache.resolve()), '--locale', args.locale]
    started = datetime.now(timezone.utc).isoformat()
    timed_out = False
    try:
        result = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=args.timeout)
    except subprocess.TimeoutExpired as error:
        def partial(value):
            return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
        timed_out = True
        result = subprocess.CompletedProcess(command, 124, partial(error.stdout),
            partial(error.stderr) + '\nOpenGL probe timed out; terminated, not a clean shutdown.\n')
    print(result.stdout, end='')
    print(result.stderr, end='')
    record = dict(schema_version=1, scope='M3 SDL3/Gx context and asset probe; no original login UI or gameplay',
                  started_at=started, completed_at=datetime.now(timezone.utc).isoformat(),
                  source_commit=manifest['source_commit'], workflow_run=manifest['workflow_run'],
                  binary_sha256=actual, preset=manifest['preset'], os=platform.platform(),
                  machine=platform.machine(), sdl3=manifest.get('sdl3'), video='x11',
                  original_data_requested=bool(args.data), locale=args.locale if args.data else None,
                  environment={key: environment.get(key) for key in ['SDL_VIDEODRIVER', 'EGL_PLATFORM',
                      'LIBGL_ALWAYS_SOFTWARE', 'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS', 'LD_PRELOAD', 'LD_AUDIT']},
                  exit_code=result.returncode, timed_out=timed_out,
                  stdout=result.stdout.splitlines(), stderr=result.stderr.splitlines())
    args.output.write_text(json.dumps(record, indent=2) + '\n')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
