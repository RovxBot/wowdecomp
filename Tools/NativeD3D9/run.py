"""Verify and execute a downloaded Linux SDL3/DXVK prototype; never compile."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def verify(artifact, sanitizers):
    manifest = json.loads((artifact / 'manifest.json').read_text())
    if (manifest.get('status') != 'build-and-contracts-passed' or
            manifest.get('preset') != ('linux-d3d9-asan' if sanitizers else 'linux-d3d9') or
            manifest.get('backend') != 'DXVK_NATIVE_SDL3' or
            manifest.get('target_os') != 'LINUX' or manifest.get('target_cpu') != 'X86_64'):
        raise ValueError('Expected a passing matching Linux DXVK Native artifact')
    for name, digest in manifest['files'].items():
        path = (artifact / name).resolve()
        if not path.is_relative_to(artifact) or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError('Artifact path/hash mismatch: ' + name)
    for path in [artifact / 'NativeD3D9Check', *sorted((artifact / 'lib').glob('*'))]:
        if str(path.relative_to(artifact)) not in manifest['files']:
            raise ValueError('Unrecorded executable/library')
        contents = path.read_bytes()
        if (len(contents) < 20 or contents[:6] != b'\x7fELF\x02\x01' or contents[18:20] != b'\x3e\x00'):
            raise ValueError('Expected Linux x86-64 ELF')
    if not list((artifact / 'lib').glob('libSDL3.so*')) or not list((artifact / 'lib').glob('libdxvk_d3d9.so*')):
        raise ValueError('Missing bundled SDL3/DXVK library')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['artifact', 'output']:
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--visible', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('This prototype requires Linux x86-64')
    artifact, output = args.artifact.resolve(), args.output.resolve()
    protected = [Path('/mnt/games/ChromieCraft'), Path('/mnt/games/3.3.5aClean')]
    if any(output.is_relative_to(p.resolve()) or artifact.is_relative_to(p.resolve()) for p in protected):
        parser.error('Artifacts and reports must be outside the read-only reference installations')
    if output.is_relative_to(artifact):
        parser.error('Write runtime reports outside the verified artifact')
    manifest = verify(artifact, args.sanitizers)
    output.parent.mkdir(parents=True, exist_ok=True)
    runtime = output.parent / (output.stem + '-runtime')
    runtime.mkdir(exist_ok=True)
    binary = artifact / 'NativeD3D9Check'
    binary.chmod(0o755)
    environment = os.environ.copy()
    for name in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS', 'DXVK_CONFIG', 'DXVK_CONFIG_FILE',
                 'DXVK_FILTER_DEVICE_NAME', 'DXVK_FILTER_DEVICE_UUID']:
        environment.pop(name, None)
    environment.update(LD_LIBRARY_PATH=str(artifact / 'lib'), SDL_VIDEODRIVER='x11',
                       DXVK_WSI_DRIVER='SDL3', DXVK_LOG_PATH=str(runtime),
                       XDG_CACHE_HOME=str(runtime / 'cache'))
    if args.sanitizers:
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    command = [str(binary), '--snapshot', str(runtime / 'scene.ppm')]
    if args.visible:
        command.append('--visible')
    started = datetime.now(timezone.utc).isoformat()
    timed_out = False
    try:
        process = subprocess.run(command, env=environment, cwd=runtime, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired as error:
        timed_out = True
        def decode(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        process = subprocess.CompletedProcess(command, 124, decode(error.stdout), decode(error.stderr))
    report = dict(source_commit=manifest['source_commit'], workflow_run=manifest['workflow_run'],
                  preset=manifest['preset'], files=manifest['files'], command=command,
                  started_at=started, completed_at=datetime.now(timezone.utc).isoformat(),
                  system=platform.platform(), scope=manifest['scope'], exit_code=process.returncode,
                  timed_out=timed_out, stdout=process.stdout.splitlines(), stderr=process.stderr.splitlines(),
                  environment={k: environment.get(k) for k in ['SDL_VIDEODRIVER', 'DXVK_WSI_DRIVER',
                      'LD_LIBRARY_PATH', 'ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS', 'LD_PRELOAD',
                      'LD_AUDIT', 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'LIBGL_ALWAYS_SOFTWARE']})
    report['runtime_files'] = {str(p.relative_to(runtime)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted(runtime.rglob('*')) if p.is_file()}
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(process.stdout, end='')
    print(process.stderr, end='')
    raise SystemExit(process.returncode)


if __name__ == '__main__':
    main()
