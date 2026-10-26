"""Execute a verified Actions-built integrated startup; never compile locally."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


def validate_paths(data, settings, cache, output):
    data, settings, cache, output = (p.resolve() for p in (data, settings, cache, output))
    protected = [data.parent, Path('/mnt/games/3.3.5aClean'), Path('/mnt/games/ChromieCraft')]
    for path in [settings, cache, output]:
        if any(path.is_relative_to(root.resolve()) for root in protected):
            raise ValueError('Writable roots/reports must be outside both reference installations')
    for a, b in [(data, settings), (data, cache), (settings, cache)]:
        if a.is_relative_to(b) or b.is_relative_to(a):
            raise ValueError('Data/settings/cache must be separate roots')


def verify_artifact(artifact, sanitizers):
    manifest = json.loads((artifact / 'manifest.json').read_text())
    preset = 'linux-client-asan' if sanitizers else 'linux-client'
    if (manifest.get('status') != 'build-and-contracts-passed' or manifest.get('preset') != preset or
            manifest.get('target_os') != 'LINUX' or manifest.get('target_cpu') != 'X86_64' or
            manifest.get('signatures') != 'required'):
        raise ValueError('Expected a passing matching Linux client Actions artifact with required signatures')
    binary = artifact / 'WowNativeClient'
    contents = binary.read_bytes()
    if (len(contents) < 20 or contents[:6] != b'\x7fELF\x02\x01' or contents[18:20] != b'\x3e\x00' or
            hashlib.sha256(contents).hexdigest() != manifest['binaries']['WowNativeClient']):
        raise ValueError('Native client ELF type or hosted binary hash mismatch')
    for name, digest in manifest['notices'].items():
        if Path(name).name != name or hashlib.sha256((artifact / 'notices' / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Hosted dependency notice hash mismatch')
    return manifest, binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['artifact', 'data', 'settings', 'cache', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--frames', type=int, default=0, help='0 waits for window close')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--hidden', action='store_true')
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('The integrated startup target requires Linux x86-64')
    if not 0 <= args.frames <= 10000000 or args.timeout < 1:
        parser.error('Invalid frame limit/timeout')
    validate_paths(args.data, args.settings, args.cache, args.output)
    manifest, binary = verify_artifact(args.artifact.resolve(), args.sanitizers)
    binary.chmod(0o755)
    environment = os.environ.copy()
    environment.update(SDL_VIDEODRIVER='x11', EGL_PLATFORM='x11')
    for name in ['LSAN_OPTIONS', 'LD_PRELOAD', 'LD_AUDIT']:
        environment.pop(name, None)
    if args.sanitizers:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        if shutil.which('llvm-symbolizer'):
            environment['ASAN_SYMBOLIZER_PATH'] = shutil.which('llvm-symbolizer')
    command = [str(binary), '--data', str(args.data.resolve()), '--settings', str(args.settings.resolve()),
               '--cache', str(args.cache.resolve()), '--frames', str(args.frames)]
    if args.hidden:
        command.append('--hidden')
    started = datetime.now(timezone.utc).isoformat()
    timed_out = False
    try:
        process = subprocess.run(command, text=True, capture_output=True, env=environment, timeout=args.timeout)
    except subprocess.TimeoutExpired as error:
        timed_out = True
        def text(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        process = subprocess.CompletedProcess(command, 124, text(error.stdout), text(error.stderr) + '\nNative client timed out; this is not clean shutdown.\n')
    report = dict(schema_version=1, source_commit=manifest['source_commit'], workflow_run=manifest['workflow_run'],
                  preset=manifest['preset'], binary_sha256=manifest['binaries']['WowNativeClient'],
                  started_at=started, completed_at=datetime.now(timezone.utc).isoformat(),
                  scope='integrated original Glue startup; completion/rendering must be established from observed stages',
                  exit_code=process.returncode, timed_out=timed_out, stdout=process.stdout.splitlines(), stderr=process.stderr.splitlines(),
                  environment={name: environment.get(name) for name in ['SDL_VIDEODRIVER', 'EGL_PLATFORM', 'LIBGL_ALWAYS_SOFTWARE',
                               'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'ASAN_SYMBOLIZER_PATH', 'LSAN_OPTIONS', 'LD_PRELOAD', 'LD_AUDIT']})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(process.stdout, end='')
    print(process.stderr, end='')
    raise SystemExit(process.returncode)


if __name__ == '__main__':
    main()
