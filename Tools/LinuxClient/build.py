#!/usr/bin/env python3
"""Actions-only build of the pinned complete Linux client and AppImage."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

from prepare import HERE, LOCK, digest, prepare
from package import package


def run(command):
    print('+ ' + ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), check=True)


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('All client and dependency compilation runs in GitHub Actions.')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--strict', action='store_true')
    parser.add_argument('--test-server', action='store_true')
    args = parser.parse_args()
    root = HERE.parent.parent
    suffix = ('test-server' if args.test_server else 'normal') + ('-strict' if args.strict else '-release')
    work = Path(os.environ['RUNNER_TEMP']) / ('full-linux-' + suffix)
    work.mkdir()
    output = root / 'artifacts/full-linux' / suffix
    output.mkdir(parents=True)
    source = prepare(work / 'source', work / 'upstream.tar.gz')
    build = work / 'build'
    run(['python3', HERE / 'test_source.py', source])
    flags = '-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' if args.strict else '-O3 -DNDEBUG'
    run(['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_C_FLAGS_RELEASE=' + flags, '-DCMAKE_CXX_FLAGS_RELEASE=' + flags,
         '-DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=OFF', '-DWOWDECOMP_ENABLE_VULKAN=ON',
         '-DWOWDECOMP_WASM_ADDONS=OFF', '-DWOWDECOMP_ISOLATED_SERVER_BUILD=' + ('ON' if args.test_server else 'OFF')])
    environment = os.environ.copy()
    if args.strict:
        os.environ.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    run(['cmake', '--build', build, '--target', 'WowClient', 'RovNativePolicyCheck', 'job_exit_repro', 'job_lifecycle_test', '-j', '3'])
    run(['ctest', '--test-dir', build, '--output-on-failure', '-R', '^(rov_native_policy|job_lifecycle_test|job_exit_shutdown)$'])
    metadata = dict(source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                    upstream_commit=LOCK['commit'], test_server_login=args.test_server, strict=args.strict,
                    workflow_run='https://github.com/' + os.environ['GITHUB_REPOSITORY'] + '/actions/runs/' + os.environ['GITHUB_RUN_ID'],
                    workflow_attempt=os.environ['GITHUB_RUN_ATTEMPT'], renderer='direct-vulkan', signatures=True,
                    compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                    build_os=subprocess.check_output(['lsb_release', '-ds'], text=True).strip())
    shutil.copy2(source / 'source-audit.json', output / 'source-audit.json')
    shutil.copy2(build / 'CMakeCache.txt', output / 'CMakeCache.txt')
    application = package(source, source / 'bin/Linux/WowClient64', output, metadata)
    run(['python3', HERE / 'package_check.py', application])
    print('application=' + str(application) + ' sha256=' + digest(application), flush=True)
    # The retained source is the actual adapted build input, excluding outputs.
    shutil.rmtree(source / 'bin', ignore_errors=True)
    with tarfile.open(output / 'adapted-source.tar.gz', 'w:gz') as archive:
        archive.add(source, arcname='wowdecomp-full-linux')


if __name__ == '__main__':
    main()
