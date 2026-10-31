"""Build in Actions or verify/run an engine-, SDL- and DXVK-free Vulkan control."""
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


SCOPE = 'Vulkan loader/instance/enumeration/device lifetime attribution; no rendering or client acceptance'
STRICT = dict(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
              UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clean_environment(library_path):
    environment = os.environ.copy()
    for name in ['LD_PRELOAD', 'LD_AUDIT', 'LSAN_OPTIONS', 'ASAN_OPTIONS', 'UBSAN_OPTIONS']:
        environment.pop(name, None)
    environment.update(LD_LIBRARY_PATH=str(library_path), **STRICT)
    return environment


def verify(artifact, sanitizers):
    manifest = json.loads((artifact / 'manifest.json').read_text())
    if (manifest.get('status') != 'build-and-contracts-passed' or
            manifest.get('kind') != 'VULKAN_LIFETIME_CONTROL' or
            manifest.get('sanitizers') is not sanitizers or
            manifest.get('target_os') != 'LINUX' or manifest.get('target_cpu') != 'X86_64'):
        raise ValueError('Expected a passing matching Vulkan control artifact')
    recorded = manifest['files']
    actual = {str(p.relative_to(artifact)) for p in artifact.rglob('*') if p.is_file() and p.name != 'manifest.json'}
    if actual != set(recorded) or 'VulkanLifetimeCheck' not in recorded:
        raise ValueError('Unrecorded or missing packaged files')
    for name, expected in recorded.items():
        path = (artifact / name).resolve()
        if not path.is_relative_to(artifact) or digest(path) != expected:
            raise ValueError('Artifact path/hash mismatch: ' + name)
    libraries = sorted((artifact / 'lib').glob('*'))
    if sanitizers and not any(p.name == 'libclang_rt.asan-x86_64.so' for p in libraries):
        raise ValueError('Missing shared sanitizer runtime')
    if sanitizers and 'Clang-compiler-rt-copyright' not in recorded:
        raise ValueError('Missing sanitizer runtime notice')
    for path in [artifact / 'VulkanLifetimeCheck', *libraries]:
        contents = path.read_bytes()
        if len(contents) < 20 or contents[:6] != b'\x7fELF\x02\x01' or contents[18:20] != b'\x3e\x00':
            raise ValueError('Expected Linux x86-64 ELF')
    return manifest


def mapped_libraries(directory):
    """Identify ordinary loaded files without holding a loader/driver reference."""
    paths = set()
    for maps in directory.glob('*.maps'):
        for line in maps.read_text().splitlines():
            fields = line.split(maxsplit=5)
            if len(fields) == 6 and fields[5].startswith('/'):
                paths.add(fields[5])
    result = {}
    for name in sorted(paths):
        path = Path(name)
        if not path.is_file():
            result[name] = dict(error='Mapped file no longer available')
            continue
        with path.open('rb') as source:
            if source.read(4) != b'\x7fELF':
                continue
        entry = dict(sha256=digest(path))
        if shutil.which('readelf'):
            notes = subprocess.run(['readelf', '-n', name], capture_output=True, text=True, timeout=30)
            match = re.search(r'Build ID: (\w+)', notes.stdout)
            if match:
                entry['build_id'] = match[1]
        result[name] = entry
    return result


def execute(artifact, runtime, stage, cycles, device=None):
    runtime.mkdir(parents=True, exist_ok=False)
    binary = artifact / 'VulkanLifetimeCheck'
    binary.chmod(0o755)
    environment = clean_environment(artifact / 'lib')
    command = [str(binary), '--stage', stage, '--cycles', str(cycles), '--maps', str(runtime)]
    if device:
        command += ['--device', device]
    started = datetime.now(timezone.utc).isoformat()
    timed_out = False
    try:
        process = subprocess.run(command, env=environment, cwd=runtime, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired as error:
        timed_out = True
        def decode(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        process = subprocess.CompletedProcess(command, 124, decode(error.stdout), decode(error.stderr))
    return dict(command=command, stage=stage, cycles=cycles, started_at=started,
                completed_at=datetime.now(timezone.utc).isoformat(), system=platform.platform(),
                exit_code=process.returncode, timed_out=timed_out,
                stdout=process.stdout.splitlines(), stderr=process.stderr.splitlines(),
                environment={k: environment.get(k) for k in [*STRICT, 'LSAN_OPTIONS', 'LD_PRELOAD',
                    'LD_AUDIT', 'LD_LIBRARY_PATH', 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES',
                    'VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_DISABLE', 'VK_LOADER_LAYERS_ENABLE']},
                mapped_libraries=mapped_libraries(runtime),
                runtime_files={p.name: digest(p) for p in sorted(runtime.glob('*.maps'))})


def build(args):
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('All client/dependency compilation must run in GitHub Actions.')
    root = Path(__file__).resolve().parents[2]
    mode = 'vulkan-control-asan' if args.sanitizers else 'vulkan-control'
    work, output = root / 'build' / mode, root / 'artifacts' / mode
    work.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=False)
    libraries = output / 'lib'
    libraries.mkdir()
    environment = clean_environment(libraries)
    manifest = dict(kind='VULKAN_LIFETIME_CONTROL', schema_version=1, sanitizers=args.sanitizers,
                    target_os='LINUX', target_cpu='X86_64', scope=SCOPE, status='building',
                    source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                    workflow_run=f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
                    workflow_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'))

    def run(name, command):
        with (output / (name + '.log')).open('w') as log:
            subprocess.run(command, cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)

    try:
        if args.sanitizers:
            runtime = Path(subprocess.check_output(['clang', '-print-file-name=libclang_rt.asan-x86_64.so'], text=True).strip())
            shutil.copy2(runtime, libraries / runtime.name)
            major = subprocess.check_output(['clang', '-dumpversion'], text=True).strip().split('.')[0]
            shutil.copy2(Path('/usr/share/doc') / ('libclang-rt-' + major + '-dev') / 'copyright',
                         output / 'Clang-compiler-rt-copyright')
        run('configure', ['cmake', '-S', str(root / 'Tools/NativeD3D9/VulkanControl'), '-B', str(work), '-G', 'Ninja',
            '-DCMAKE_CXX_COMPILER=clang++', '-DCMAKE_BUILD_TYPE=Release',
            '-DCONTROL_SANITIZERS=' + ('ON' if args.sanitizers else 'OFF')])
        run('compile', ['cmake', '--build', str(work), '--parallel', '2'])
        shutil.copy2(work / 'VulkanLifetimeCheck', output / 'VulkanLifetimeCheck')
        run('elf-needed', ['readelf', '-d', str(output / 'VulkanLifetimeCheck')])
        run('toolchain', ['clang', '--version'])
        run('system-packages', ['dpkg-query', '-W', 'libvulkan1', 'libvulkan-dev', 'mesa-vulkan-drivers'])
        icds = list(Path('/usr/share/vulkan/icd.d').glob('*lvp*.json'))
        if len(icds) != 1:
            raise ValueError('Expected one hosted lavapipe ICD')
        # A headless control: no SDL/window/display dependency, no private driver.
        os.environ['VK_DRIVER_FILES'] = str(icds[0])
        os.environ.pop('VK_ICD_FILENAMES', None)
        manifest['hosted_icd'] = dict(path=str(icds[0]), sha256=digest(icds[0]), contents=json.loads(icds[0].read_text()))
        failures = []
        for stage, cycles in [('loader', 1), *[(s, c) for s in ['instance', 'enumerate', 'device'] for c in [1, 3]]]:
            report = execute(output, output / f'{stage}-{cycles}-maps', stage, cycles)
            (output / f'{stage}-{cycles}.json').write_text(json.dumps(report, indent=2) + '\n')
            print(stage, cycles, 'exit', report['exit_code'], flush=True)
            if report['exit_code']:
                failures.append(f'{stage}/{cycles}')
        report = execute(output, output / 'missing-device-maps', 'device', 1, 'ffff:ffff')
        (output / 'missing-device.json').write_text(json.dumps(report, indent=2) + '\n')
        if (report['exit_code'] != 1 or report['timed_out'] or
                report['stderr'] != ['Vulkan lifetime control failed: Requested physical device not found']):
            failures.append('missing-device unwind')
        if failures:
            raise RuntimeError('Failed controls: ' + ', '.join(failures))
        manifest['status'] = 'build-and-contracts-passed'
    except Exception as error:
        manifest.update(status='failed', failure=str(error))
        raise
    finally:
        manifest['files'] = {str(p.relative_to(output)): digest(p) for p in sorted(output.rglob('*'))
                             if p.is_file() and p.name != 'manifest.json'}
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='operation', required=True)
    builder = commands.add_parser('build')
    runner = commands.add_parser('run')
    for command in [builder, runner]:
        command.add_argument('--sanitizers', action='store_true')
    for name in ['artifact', 'output']:
        runner.add_argument('--' + name, type=Path, required=True)
    runner.add_argument('--stage', choices=['loader', 'instance', 'enumerate', 'device'], default='device')
    runner.add_argument('--cycles', type=int, choices=range(1, 33), default=3)
    runner.add_argument('--device', help='Require a unique hexadecimal PCI vendor:device (device stage only)')
    args = parser.parse_args()
    if args.operation == 'build':
        build(args)
        return
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('This control requires Linux x86-64')
    artifact, output = args.artifact.resolve(), args.output.resolve()
    for reference in [Path('/mnt/games/ChromieCraft'), Path('/mnt/games/3.3.5aClean')]:
        if artifact.is_relative_to(reference.resolve()) or output.is_relative_to(reference.resolve()):
            parser.error('Artifacts and reports must be outside the read-only reference installations')
    if output.is_relative_to(artifact):
        parser.error('Write runtime reports outside the verified artifact')
    if args.device and args.stage != 'device':
        parser.error('--device requires --stage device')
    manifest = verify(artifact, args.sanitizers)
    output.parent.mkdir(parents=True, exist_ok=True)
    report = execute(artifact, output.parent / (output.stem + '-maps'), args.stage, args.cycles, args.device)
    report.update(source_commit=manifest['source_commit'], workflow_run=manifest['workflow_run'],
                  workflow_attempt=manifest['workflow_attempt'], scope=SCOPE, files=manifest['files'])
    output.write_text(json.dumps(report, indent=2) + '\n')
    print('\n'.join(report['stdout']))
    print('\n'.join(report['stderr']))
    raise SystemExit(report['exit_code'])


if __name__ == '__main__':
    main()
