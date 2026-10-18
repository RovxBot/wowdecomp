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
    parser.add_argument('--render-target-checks', action='store_true',
                        help='Also hash-verify and run the three-cycle native Gx target fixture')
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
    args.output.unlink(missing_ok=True)
    manifest = json.loads((args.artifact / 'manifest.json').read_text())
    if manifest['preset'] not in {'linux-gl', 'linux-gl-asan'} or manifest['target_os'] != 'LINUX':
        parser.error('Expected a Linux M3 OpenGL Actions artifact')
    binary = args.artifact / 'bin/NativeGlCheck'
    actual = hashlib.sha256(binary.read_bytes()).hexdigest()
    expected = next(item['sha256'] for item in manifest['binaries'] if item['name'] == 'NativeGlCheck')
    if actual != expected:
        raise SystemExit('Hosted OpenGL binary hash does not match the manifest')
    binary.chmod(0o755)
    target_binary = None
    if args.render_target_checks:
        target_binary = args.artifact / 'bin/GlRenderTargetsCheck'
        entry = next((item for item in manifest['binaries'] if item['name'] == 'GlRenderTargetsCheck'), None)
        if entry is None or not target_binary.is_file():
            raise SystemExit('Hosted artifact does not contain native render-target checks')
        target_hash = hashlib.sha256(target_binary.read_bytes()).hexdigest()
        if target_hash != entry['sha256']:
            raise SystemExit('Hosted render-target binary hash does not match the manifest')
        target_binary.chmod(0o755)
    environment = os.environ.copy()
    environment.update(SDL_VIDEODRIVER='x11', EGL_PLATFORM='x11')
    if args.sanitizers or manifest['preset'].endswith('-asan'):
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    command = [str(binary.resolve()), '--frames', str(args.frames), '--cycles', str(args.cycles)]
    if args.snapshot:
        args.snapshot.unlink(missing_ok=True)
        command += ['--snapshot', str(args.snapshot.resolve())]
    if args.data:
        command += ['--data', str(args.data.resolve()), '--settings', str(args.settings.resolve()),
                    '--cache', str(args.cache.resolve()), '--locale', args.locale]
    started = datetime.now(timezone.utc).isoformat()
    def execute(arguments):
        try:
            return subprocess.run(arguments, env=environment, capture_output=True,
                                  text=True, timeout=args.timeout), False
        except subprocess.TimeoutExpired as error:
            def partial(value):
                return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''
            return subprocess.CompletedProcess(arguments, 124, partial(error.stdout),
                partial(error.stderr) + '\nOpenGL probe timed out; terminated, not a clean shutdown.\n'), True
    target_record = None
    if target_binary:
        target_result, target_timeout = execute([str(target_binary.resolve())])
        print(target_result.stdout, end='')
        print(target_result.stderr, end='')
        target_record = dict(binary_sha256=target_hash, exit_code=target_result.returncode,
                             timed_out=target_timeout, stdout=target_result.stdout.splitlines(),
                             stderr=target_result.stderr.splitlines())
    result, timed_out = execute(command)
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
    if target_record is not None:
        record['render_target_checks'] = target_record
        record['probe_exit_code'] = result.returncode
        record['exit_code'] = target_record['exit_code'] or result.returncode
        record['timed_out'] = target_record['timed_out'] or timed_out
    args.output.write_text(json.dumps(record, indent=2) + '\n')
    raise SystemExit(record['exit_code'])


if __name__ == '__main__':
    main()
