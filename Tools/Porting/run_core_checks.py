"""Compile the M1 core only in GitHub Actions and retain reproducible evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

from audit_build_graph import audit, read_targets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', choices=['linux-core', 'linux-core-asan', 'macos-arm64-core', 'windows-core'], required=True)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('Client/dependency compilation must run in GitHub Actions.')
    root = Path(__file__).resolve().parents[2]
    os.chdir(root)
    evidence = root / 'artifacts' / args.preset
    evidence.mkdir(parents=True, exist_ok=True)
    build = root / 'build' / args.preset
    query = build / '.cmake/api/v1/query/codemodel-v2'
    query.parent.mkdir(parents=True, exist_ok=True)
    query.touch()
    target_os, cpu = {'linux-core': ('LINUX', 'X86_64'),
                      'linux-core-asan': ('LINUX', 'X86_64'),
                      'macos-arm64-core': ('MACOS', 'ARM64'),
                      'windows-core': ('WINDOWS', 'X86_64')}[args.preset]
    manifest = {
        'schema_version': 1, 'scope': 'M1 core services and native lifecycle; no game assets/window/network/gameplay',
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'workflow_run': f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        'preset': args.preset, 'target_os': target_os, 'target_cpu': cpu,
        'runner_os': os.environ.get('RUNNER_OS'), 'runner_arch': os.environ.get('RUNNER_ARCH'),
        'runner_image': os.environ.get('ImageVersion'), 'host_machine': platform.machine(),
        'status': 'preflight',
    }

    def run(name, command):
        manifest['status'] = name
        with (evidence / f'{name}.log').open('w') as log:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, bufsize=1)
            for line in process.stdout:
                sys.stdout.write(line)
                log.write(line)
            if process.wait():
                raise RuntimeError(f'{name} failed: {command}')

    try:
        if cpu == 'ARM64' and (platform.machine() != 'arm64' or os.environ.get('RUNNER_ARCH') != 'ARM64'):
            raise RuntimeError('The macOS core check requires an actual ARM64 runner.')
        run('cmake-version', ['cmake', '--version'])
        run('configure', ['cmake', '--preset', args.preset])
        targets = read_targets(build)
        errors = audit(targets, 'core', target_os, cpu)
        (evidence / 'build-graph.json').write_text(json.dumps({'errors': errors, 'targets': targets}, indent=2) + '\n')
        if errors:
            raise RuntimeError('\n'.join(errors))
        # Preserve cache/toolchain identity; no game data or credentials enter this build.
        cache = (build / 'CMakeCache.txt').read_text()
        manifest['toolchain_cache'] = [line for line in cache.splitlines() if line.startswith(
            ('CMAKE_CXX_COMPILER:', 'CMAKE_C_COMPILER:', 'CMAKE_MAKE_PROGRAM:', 'CMAKE_OSX_ARCHITECTURES:'))]
        run('build', ['cmake', '--build', str(build), '--config', 'Release', '--target',
                      'WowPortableCore', 'PortableCoreCheck', 'CoreServicesCheck', 'NativeCore', '--parallel', '2'])
        run('tests', ['ctest', '--test-dir', str(build), '-C', 'Release', '--output-on-failure', '--no-tests=error'])
        executable = build / 'bin' / ('Release/PortableCoreCheck.exe' if target_os == 'WINDOWS' else 'PortableCoreCheck')
        if target_os == 'MACOS':
            architectures = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).strip()
            manifest['binary_architectures'] = architectures
            if architectures != 'arm64':
                raise RuntimeError(f'Expected an ARM64 executable, found {architectures}')
        if target_os != 'WINDOWS':
            run('binary-type', ['file', str(executable)])
        manifest['check_executable_sha256'] = hashlib.sha256(executable.read_bytes()).hexdigest()
        manifest['status'] = 'core services, semantic checks and minimal native lifecycle passed; full game client unported'
    except Exception as exc:
        manifest['failed_phase'] = manifest['status']
        manifest['status'] = 'failed'
        manifest['failure'] = str(exc)
        raise
    finally:
        (evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
