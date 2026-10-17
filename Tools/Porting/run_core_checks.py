"""Compile the allowlisted core/file subsets only in Actions; retain evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from audit_build_graph import audit, read_targets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset', choices=['linux-core', 'linux-core-asan', 'macos-arm64-core', 'windows-core',
                                           'linux-files', 'linux-files-asan', 'macos-arm64-files', 'windows-files',
                                           'linux-assets', 'linux-assets-asan', 'macos-arm64-assets', 'windows-assets',
                                           'linux-async', 'linux-async-asan', 'macos-arm64-async', 'windows-async',
                                           'linux-window', 'linux-window-asan', 'macos-arm64-window', 'windows-window'], required=True)
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
    window = '-window' in args.preset
    asynchronous = '-async' in args.preset or window
    assets = '-assets' in args.preset or asynchronous
    files = '-files' in args.preset or assets
    core_preset = args.preset.replace('-files', '-core').replace('-assets', '-core').replace('-async', '-core').replace('-window', '-core')
    target_os, cpu = {'linux-core': ('LINUX', 'X86_64'),
                      'linux-core-asan': ('LINUX', 'X86_64'),
                      'macos-arm64-core': ('MACOS', 'ARM64'),
                      'windows-core': ('WINDOWS', 'X86_64')}[core_preset]
    manifest = {
        'schema_version': 1, 'scope': ('M2 SDL3 window/input synthetic fixtures plus native async/assets/core; dummy driver, no game' if window else
                                    'M2 native asynchronous file, package/asset and core fixtures; no window/gameplay' if asynchronous else
                                    'M2 native read-only MPQ/signatures with synthetic fixtures; no game/window' if assets else
                                    'M2 native host files/paths plus M1 core; synthetic fixtures, no archives/async/window/gameplay' if files else
                                     'M1 core services and native lifecycle; no game assets/window/network/gameplay'),
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
        errors = audit(targets, 'window' if window else 'async' if asynchronous else 'assets' if assets else 'files' if files else 'core', target_os, cpu)
        (evidence / 'build-graph.json').write_text(json.dumps({'errors': errors, 'targets': targets}, indent=2) + '\n')
        if errors:
            raise RuntimeError('\n'.join(errors))
        # Preserve cache/toolchain identity; no game data or credentials enter this build.
        cache = (build / 'CMakeCache.txt').read_text()
        manifest['toolchain_cache'] = [line for line in cache.splitlines() if line.startswith(
            ('CMAKE_CXX_COMPILER:', 'CMAKE_C_COMPILER:', 'CMAKE_MAKE_PROGRAM:', 'CMAKE_OSX_ARCHITECTURES:'))]
        executables = ['PortableCoreCheck', 'CoreServicesCheck', 'NativeCore'] + (['FileServicesCheck'] if files else []) + (['PackageServicesCheck', 'AssetDecodersCheck', 'NativeAssetCheck'] if assets else []) + (['AsyncFileCheck'] if asynchronous else []) + (['NativeWindowCheck', 'ShellSettingsCheck', 'NativeShell'] if window else [])
        run('build', ['cmake', '--build', str(build), '--config', 'Release', '--target',
                      'WowPortableCore', *executables, '--parallel', '2'])
        run('tests', ['ctest', '--test-dir', str(build), '-C', 'Release', '--output-on-failure', '--no-tests=error'])
        manifest['binaries'] = []
        for name in executables:
            relative = f'Release/{name}.exe' if target_os == 'WINDOWS' else name
            executable = build / 'bin' / relative
            entry = {'name': name, 'sha256': hashlib.sha256(executable.read_bytes()).hexdigest()}
            if target_os == 'MACOS':
                architectures = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).strip()
                entry['binary_architectures'] = architectures
                if architectures != 'arm64':
                    raise RuntimeError(f'Expected an ARM64 executable, found {architectures}')
            if target_os != 'WINDOWS':
                run(f'binary-type-{name}', ['file', str(executable)])
            manifest['binaries'].append(entry)
            if name in {'NativeCore', 'FileServicesCheck', 'PackageServicesCheck', 'NativeAssetCheck', 'NativeShell', 'NativeWindowCheck'}:
                binary_directory = evidence / 'bin'
                binary_directory.mkdir(exist_ok=True)
                shutil.copy2(executable, binary_directory / executable.name)
        if window:
            notices = evidence / 'notices'
            notices.mkdir(exist_ok=True)
            shutil.copy2(build / '_deps/sdl3-src/LICENSE.txt', notices / 'SDL3-LICENSE.txt')
            manifest['sdl3'] = {'version': '3.2.28', 'commit': '7f3ae3d57459e59943a4ecfefc8f6277ec6bf540',
                                'archive_sha256': '7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5'}
        manifest['status'] = ('native SDL3 window/input fixtures passed on dummy video; desktop validation separate' if window else
                              'native asynchronous file and package/asset fixtures passed; no full game/window' if asynchronous else
                              'native MPQ/signature fixtures passed; original-data runtime validation separate' if assets else
                              'native host-file/path fixtures and core lifecycle passed; M2 archives/async/SDL not yet integrated' if files else
                              'core services, semantic checks and minimal native lifecycle passed; full game client unported')
    except Exception as exc:
        manifest['failed_phase'] = manifest['status']
        manifest['status'] = 'failed'
        manifest['failure'] = str(exc)
        raise
    finally:
        last_test = build / 'Testing' / 'Temporary' / 'LastTest.log'
        if last_test.exists():
            shutil.copy2(last_test, evidence / 'ctest-detail.log')
        (evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
