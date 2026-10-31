"""Actions-only, pinned SDL3/DXVK Native build and authored Linux qualification."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
from urllib.request import urlopen


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise SystemExit('All client/dependency compilation must run in GitHub Actions.')
    root = Path(__file__).resolve().parents[2]
    mode = 'linux-d3d9-asan' if args.sanitizers else 'linux-d3d9'
    work = root / 'build' / mode
    output = root / 'artifacts' / mode
    work.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    prefix = work / 'prefix'
    sources = work / 'sources'
    pins = json.loads((root / 'Tools/NativeD3D9/dependencies.json').read_text())
    environment = os.environ.copy()
    environment.update(CC='clang', CXX='clang++', PKG_CONFIG_PATH=str(prefix / 'lib/pkgconfig'),
                       LD_LIBRARY_PATH=str(prefix / 'lib'), SDL_VIDEODRIVER='x11',
                       ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
                       DXVK_LOG_PATH=str(output), XDG_CACHE_HOME=str(work / 'cache'))
    manifest = dict(schema_version=1, source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        workflow_run=f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        workflow_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'),
        preset=mode, target_os='LINUX', target_cpu='X86_64', backend='DXVK_NATIVE_SDL3',
        dependencies=pins, status='building',
        scope='authored standalone ABI/WSI fixture; no original Gx/data/login/gameplay qualification')

    def run(name, command, cwd=root):
        with (output / (name + '.log')).open('w') as log:
            process = subprocess.Popen(command, cwd=cwd, env=environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            for line in process.stdout:
                print(line, end='', flush=True)
                log.write(line)
            if process.wait():
                raise RuntimeError(name + ' failed')

    try:
        for pin in pins:  # Parent DXVK must precede its submodule destinations.
            destination = sources / pin['destination']
            if destination.exists():
                shutil.rmtree(destination)
            with urlopen(pin['url'], timeout=120) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != pin['sha256']:
                raise ValueError('Dependency archive hash mismatch: ' + pin['name'])
            unpack = work / 'unpack'
            unpack.mkdir()
            with tarfile.open(fileobj=io.BytesIO(data)) as archive:
                archive.extractall(unpack, filter='data')
            entries = list(unpack.iterdir())
            if len(entries) != 1 or not entries[0].is_dir():
                raise ValueError('Expected one dependency archive root')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(entries[0], destination)
            unpack.rmdir()
        patches = sorted((root / 'Tools/NativeD3D9').glob('dxvk-*.patch'))
        manifest['dependency_patches'] = {patch.name: digest(patch) for patch in patches}
        for patch in patches:
            run(patch.stem + '-check', ['patch', '--dry-run', '--batch', '--fuzz=0', '-p1', '-i', str(patch)], cwd=sources / 'dxvk')
            run(patch.stem + '-apply', ['patch', '--batch', '--fuzz=0', '-p1', '-i', str(patch)], cwd=sources / 'dxvk')
        manifest['adapted_sha1_sha256'] = digest(sources / 'dxvk/src/util/sha1/sha1.c')
        manifest['adapted_sdl3_imports_sha256'] = digest(sources / 'dxvk/src/wsi/sdl3/wsi_platform_sdl3_funcs.h')
        if args.sanitizers:
            # SDL's shared target requires resolved symbols. Link all three
            # consumers to one Clang sanitizer runtime, also shipped in the ZIP.
            runtime = Path(subprocess.check_output(
                ['clang', '-print-file-name=libclang_rt.asan-x86_64.so'], text=True).strip())
            (prefix / 'lib').mkdir(parents=True, exist_ok=True)
            shutil.copy2(runtime, prefix / 'lib' / runtime.name)
        flags = '-fsanitize=address,undefined -shared-libasan -fno-omit-frame-pointer -g1' if args.sanitizers else ''
        run('sdl-configure', ['cmake', '-S', str(sources / 'sdl3'), '-B', str(work / 'sdl'), '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_PREFIX=' + str(prefix), '-DCMAKE_INSTALL_LIBDIR=lib',
            '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++', '-DCMAKE_C_FLAGS=' + flags,
            '-DCMAKE_SHARED_LINKER_FLAGS=' + flags, '-DSDL_SHARED=ON', '-DSDL_STATIC=OFF',
            '-DSDL_TEST_LIBRARY=OFF', '-DSDL_TESTS=OFF', '-DSDL_EXAMPLES=OFF',
            '-DSDL_X11_SHARED=OFF', '-DSDL_WAYLAND_SHARED=OFF', '-DSDL_WAYLAND_LIBDECOR_SHARED=OFF',
            '-DSDL_AUDIO=OFF', '-DSDL_CAMERA=OFF', '-DSDL_JOYSTICK=OFF', '-DSDL_HAPTIC=OFF',
            '-DSDL_SENSOR=OFF', '-DSDL_HIDAPI=OFF', '-DSDL_POWER=OFF', '-DSDL_GPU=OFF',
            '-DSDL_RENDER=OFF', '-DSDL_OPENGL=OFF', '-DSDL_OPENGLES=OFF', '-DSDL_VULKAN=ON'])
        run('sdl-compile', ['cmake', '--build', str(work / 'sdl'), '--parallel', '4'])
        run('sdl-install', ['cmake', '--install', str(work / 'sdl')])
        run('dxvk-configure', ['meson', 'setup', str(work / 'dxvk'), str(sources / 'dxvk'),
            '--prefix=' + str(prefix), '--libdir=lib', '--buildtype=release', '--wrap-mode=nofallback',
            '--force-fallback-for=libdisplay-info', '-Denable_d3d9=true', '-Denable_d3d8=false',
            '-Denable_d3d10=false', '-Denable_d3d11=false', '-Denable_dxgi=false',
            '-Dnative_sdl3=enabled', '-Dnative_sdl2=disabled', '-Dnative_glfw=disabled',
            '-Db_sanitize=' + ('address,undefined' if args.sanitizers else 'none'),
            '-Dc_link_args=' + ('-shared-libasan' if args.sanitizers else ''),
            '-Dcpp_link_args=' + ('-shared-libasan' if args.sanitizers else '')])
        run('dxvk-compile', ['meson', 'compile', '-C', str(work / 'dxvk'), '-j', '4'])
        run('dxvk-install', ['meson', 'install', '-C', str(work / 'dxvk')])
        run('probe-configure', ['cmake', '-S', str(root / 'Tools/NativeD3D9'), '-B', str(work / 'probe'),
            '-G', 'Ninja', '-DCMAKE_CXX_COMPILER=clang++', '-DCMAKE_BUILD_TYPE=Release',
            '-DDXVK_SOURCE_DIR=' + str(sources / 'dxvk'),
            '-DPROTOTYPE_SANITIZERS=' + ('ON' if args.sanitizers else 'OFF')])
        run('probe-compile', ['cmake', '--build', str(work / 'probe'), '--parallel', '4'])
        shutil.copy2(work / 'probe/NativeD3D9Check', output / 'NativeD3D9Check')
        shutil.copy2(work / 'probe/DxvkSha1Check', output / 'DxvkSha1Check')
        (output / 'lib').mkdir(exist_ok=True)
        for pattern in ['libSDL3.so*', 'libdxvk_d3d9.so*', 'libclang_rt.asan-*.so']:
            for path in (prefix / 'lib').glob(pattern):
                if not path.is_symlink():
                    shutil.copy2(path, output / 'lib' / path.name)
        # Preserve SONAME symlinks as regular files: Actions ZIPs do not retain links.
        for path in (prefix / 'lib').glob('*.so*'):
            if path.is_symlink() and path.resolve().name.startswith(('libSDL3', 'libdxvk_d3d9')):
                shutil.copy2(path, output / 'lib' / path.name)
        notices = output / 'notices'
        notices.mkdir(exist_ok=True)
        if args.sanitizers:
            clang_major = subprocess.check_output(['clang', '-dumpversion'], text=True).strip().split('.')[0]
            shutil.copy2(Path('/usr/share/doc') / ('libclang-rt-' + clang_major + '-dev') / 'copyright',
                         notices / 'Clang-compiler-rt-copyright')
        for directory, name in [('sdl3', 'LICENSE.txt'), ('dxvk', 'LICENSE'),
                                ('dxvk/include/native/directx', 'COPYING.MinGW-w64.txt'),
                                ('dxvk/include/vulkan', 'LICENSE.md'), ('dxvk/include/spirv', 'LICENSE'),
                                ('dxvk/subprojects/libdisplay-info', 'LICENSE')]:
            shutil.copy2(sources / directory / name, notices / (directory.replace('/', '-') + '-' + name))
        shutil.copytree(sources / 'dxvk/include/vulkan/LICENSES', notices / 'vulkan-LICENSES', dirs_exist_ok=True)
        for path in (sources / 'dxvk/include/native/directx').glob('COPYING*'):
            shutil.copy2(path, notices / ('directx-' + path.name))
        shutil.copy2(sources / 'sdl3/src/video/yuv2rgb/LICENSE', notices / 'SDL-yuv2rgb-LICENSE')
        shutil.copy2(sources / 'dxvk/include/openvr/LICENSE', notices / 'openvr-LICENSE')
        shutil.copy2(sources / 'dxvk/src/util/sha1/sha1.c', notices / 'DXVK-SHA1-adapted.c')
        shutil.copy2(sources / 'dxvk/src/wsi/sdl3/wsi_platform_sdl3_funcs.h', notices / 'DXVK-SDL3-adapted.h')
        for patch in patches:
            shutil.copy2(patch, notices / patch.name)
        environment['LD_LIBRARY_PATH'] = str(output / 'lib')
        run('sha1-contracts', [str(output / 'DxvkSha1Check')], cwd=output)
        run('contracts', [str(output / 'NativeD3D9Check'), '--snapshot', str(output / 'scene.ppm')], cwd=output)
        run('toolchain', ['sh', '-c', 'clang --version && cmake --version && meson --version && glslangValidator --version'])
        manifest['status'] = 'build-and-contracts-passed'
    except Exception as error:
        manifest['status'] = 'failed'
        manifest['failure'] = str(error)
        raise
    finally:
        manifest['files'] = {str(p.relative_to(output)): digest(p) for p in sorted(output.rglob('*'))
                             if p.is_file() and p.name != 'manifest.json'}
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
