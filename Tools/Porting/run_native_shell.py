"""Execute a hash-verified hosted shell with external data; never compile locally."""
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
    for argument in ['artifact', 'data', 'settings', 'cache', 'output']:
        parser.add_argument('--' + argument, type=Path, required=True)
    parser.add_argument('--locale', default='enUS')
    parser.add_argument('--smoke-ms', type=int)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--video', choices=['x11', 'wayland', 'cocoa', 'windows', 'dummy'])
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--timeout', type=int, default=600)
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error('--timeout must be positive')
    manifest = json.loads((args.artifact/'manifest.json').read_text())
    name = 'NativeShell.exe' if platform.system() == 'Windows' else 'NativeShell'
    binary = args.artifact/'bin'/name
    actual = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = next(item['sha256'] for item in manifest['binaries'] if item['name'] == 'NativeShell')
    if actual != expected:
        raise SystemExit('Hosted shell hash does not match its manifest')
    if platform.system() != 'Windows':
        binary.chmod(0o755)
    environment = os.environ.copy()
    if args.video:
        environment['SDL_VIDEODRIVER'] = args.video
        # A Wayland EGL override inherited from the desktop can make Mesa treat
        # SDL's X11 Display pointer as a wl_display. Match this explicit request
        # in the child only; leave the desktop and automatic selection unchanged.
        if platform.system() == 'Linux' and args.video in {'x11', 'wayland'}:
            environment['EGL_PLATFORM'] = args.video
    if args.sanitizers:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    report = args.output.with_suffix('.session.txt')
    # Never attach a previous successful session to a failed or timed-out launch.
    report.unlink(missing_ok=True)
    command = [str(binary.resolve()), '--data', str(args.data), '--locale', args.locale,
               '--settings', str(args.settings), '--cache', str(args.cache), '--report', str(report)]
    if args.smoke_ms:
        command += ['--smoke-ms', str(args.smoke_ms)]
    if args.snapshot:
        command += ['--snapshot', str(args.snapshot)]
    started = datetime.now(timezone.utc).isoformat()
    timed_out = False
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout, env=environment)
    except subprocess.TimeoutExpired as error:
        # subprocess.run kills and reaps the child before raising. Partial output
        # can still be bytes even with text=True, including a partial UTF-8 scalar.
        def partial(value):
            return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
        result = subprocess.CompletedProcess(command, 124, partial(error.stdout), partial(error.stderr))
        result.stderr += f'\nNativeShell exceeded {args.timeout}s; terminated, not a clean shutdown/input pass.\n'
        timed_out = True
    print(result.stdout, end='')
    print(result.stderr, end='')
    record = dict(scope='Native M2 diagnostic shell; external 12340 data; no Gx/gameplay',
                  started_at=started, completed_at=datetime.now(timezone.utc).isoformat(),
                  source_commit=manifest['source_commit'], hosted_build_run=manifest['workflow_run'],
                  binary_sha256=actual, sdl3=manifest.get('sdl3'),
                  os=platform.platform(), machine=platform.machine(), locale=args.locale,
                  data_root='external read-only reference',
                  requested_video=environment.get('SDL_VIDEODRIVER'),
                  process_environment={key: environment.get(key) for key in
                                       ['EGL_PLATFORM', 'SDL_VIDEO_X11_XINPUT2', 'LIBDECOR_PLUGIN_DIR']},
                  sanitizer_options={key: environment.get(key) for key in ['ASAN_OPTIONS', 'UBSAN_OPTIONS']},
                  exit_code=result.returncode, timed_out=timed_out,
                  stdout=result.stdout.splitlines(), stderr=result.stderr.splitlines())
    if report.exists():
        record['session'] = report.read_text().splitlines()
    args.output.write_text(json.dumps(record, indent=2)+'\n')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
