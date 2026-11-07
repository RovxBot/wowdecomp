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
from distribute import distribution


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
    shutil.copy2(source / 'source-audit.json', output / 'source-audit.json')
    build = work / 'build'
    run(['python3', HERE / 'test_source.py', source])
    flags = '-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' if args.strict else '-O3 -DNDEBUG'
    run(['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_C_FLAGS_RELEASE=' + flags, '-DCMAKE_CXX_FLAGS_RELEASE=' + flags,
         '-DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=OFF', '-DWOWDECOMP_ENABLE_VULKAN=ON',
         '-DWOWDECOMP_WASM_ADDONS=OFF', '-DWOWDECOMP_ISOLATED_SERVER_BUILD=' + ('ON' if args.test_server else 'OFF')])
    if args.strict:
        os.environ.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    run(['cmake', '--build', build, '--target', 'WowClient', 'RovNativePolicyCheck', 'RovNativeNearCheck', 'RovNativeAngleCheck', 'RovNativeAttributesCheck', 'RovNativeFreeTypeStreamCheck', 'RovNativeM2CacheSeedCheck', 'RovNativeLuaUpvalueCheck', 'RovNativeALSAOutputCheck', 'job_exit_repro', 'job_lifecycle_test', '-j', '3'])
    run(['ctest', '--test-dir', build, '--output-on-failure', '-R', '^(rov_native_policy|rov_native_near_memory|rov_native_audio_angle|rov_native_packed_attributes|rov_native_freetype_stream|rov_native_m2_cache_seed|rov_native_lua_upvalue|rov_native_alsa_output|job_lifecycle_test|job_exit_shutdown)$'])
    metadata = dict(source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                    upstream_commit=LOCK['commit'], test_server_login=args.test_server, strict=args.strict,
                    workflow_run='https://github.com/' + os.environ['GITHUB_REPOSITORY'] + '/actions/runs/' + os.environ['GITHUB_RUN_ID'],
                    workflow_attempt=os.environ['GITHUB_RUN_ATTEMPT'], renderer='direct-vulkan', signatures=True,
                    compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                    build_os=subprocess.check_output(['lsb_release', '-ds'], text=True).strip())
    shutil.copy2(source / 'source-audit.json', output / 'source-audit.json')
    shutil.copy2(build / 'CMakeCache.txt', output / 'CMakeCache.txt')
    # Retain exact adapted inputs even if executable/package qualification fails.
    with tarfile.open(output / 'adapted-source.tar.gz', 'w:gz') as archive:
        def source_only(info):
            return None if info.name.startswith('wowdecomp-full-linux/bin/') else info
        archive.add(source, arcname='wowdecomp-full-linux', filter=source_only)
    application = package(source, source / 'bin/Linux/WowClient64', output, metadata)
    run(['python3', HERE / 'package_check.py', application])
    if args.test_server and not args.strict:
        archive = distribution(application)
        run(['python3', HERE / 'package_check.py', application, '--archive', archive])
    print('application=' + str(application) + ' sha256=' + digest(application), flush=True)


if __name__ == '__main__':
    main()
