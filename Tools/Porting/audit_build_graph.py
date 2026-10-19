"""Check actual CMake File API source/definition selection; no compilation here."""
import argparse
import json
from pathlib import Path


def audit(targets, mode, target_os, cpu):
    errors = []
    compiled = [t for t in targets if t['type'] in
                ('STATIC_LIBRARY', 'SHARED_LIBRARY', 'EXECUTABLE', 'OBJECT_LIBRARY')]
    build_utilities = {t['id'] for t in targets if t['type'] == 'UTILITY' and 'id' in t}
    names = {t['name'] for t in compiled}
    expected = {'WowPortableCore', 'PortableCoreCheck', 'WowCoreServices', 'CoreServicesCheck', 'NativeCore'}
    if mode == 'packets':
        expected |= {'WowNativePackets', 'NativePacketCheck'}
    if mode in {'files', 'assets', 'async', 'window', 'gl'}:
        expected |= {'WowNativeFiles', 'FileServicesCheck'}
    if mode in {'assets', 'async', 'window', 'gl'}:
        expected |= {'WowAssetCodecs', 'WowNativeAssets', 'PackageServicesCheck', 'AssetDecodersCheck', 'NativeAssetCheck'}
    if mode in {'async', 'window', 'gl'}:
        expected |= {'WowNativeAsync', 'AsyncFileCheck'}
    if mode in {'window', 'gl'}:
        expected |= {'SDL3-static', 'SDL_uclibc', 'SDLDesktopControl', 'WowNativeWindow', 'NativeWindowCheck', 'NativeShell', 'ShellSettingsCheck'}
    if mode == 'gl':
        expected |= {'WowNativeGxu', 'LegacyGxuCheck', 'WowNativeFonts', 'NativeFontsCheck', 'WowNativeGl', 'NativeGlCheck', 'ShaderPackageCheck', 'GlRenderTargetsCheck', 'GlTransformsCheck', 'GlTexturesCheck', 'GlBuffersCheck', 'LegacyGxCheck'}
    if mode in {'core', 'files', 'assets', 'async', 'window', 'gl', 'packets'} and names != expected:
        errors.append(f'Unexpected {mode} targets: {sorted(names)}')
    if mode == 'windows' and not {'Wow', 'WowClient', 'Os', 'Storm', 'BlizzardCore'} <= names:
        errors.append('Windows reference targets are missing')
    expected_platform = 'Win64' if cpu == 'X86_64' else 'Win32'
    for target in compiled:
        sources = [s['path'].replace('\\', '/') for s in target.get('sources', [])]
        definitions = {d['define'] for g in target.get('compileGroups', [])
                       for d in g.get('defines', [])}
        required = {f'WOWDECOMP_OS_{target_os}=1', f'WOWDECOMP_CPU_{cpu}=1',
                    'WOWDECOMP_ARCH_64=1' if cpu != 'X86' else 'WOWDECOMP_ARCH_32=1'}
        if not required <= definitions:
            errors.append(f"{target['name']}: missing target identity: {sorted(required - definitions)}")
        if (mode == 'windows' and target['name'] in {'Wow', 'WowClient', 'Os', 'Storm', 'BlizzardCore'}
                and 'WOWDECOMP_PORTABLE_RUNTIME=1' in definitions):
            errors.append(f"{target['name']}: portable runtime ABI leaked into the legacy client")
        for source in sources:
            parts = Path(source).parts
            if mode in {'core', 'files', 'assets', 'async', 'window', 'gl', 'packets'} and (set(parts) & {'Win32', 'Win64', 'Windows', 'win32'}
                                   or source.endswith(('.rc', '.def', '.lib'))):
                errors.append(f"{target['name']}: Windows source in core: {source}")
            if mode == 'windows' and (('Win32' in parts and expected_platform != 'Win32')
                                      or ('Win64' in parts and expected_platform != 'Win64')):
                errors.append(f"{target['name']}: wrong Windows source: {source}")
        if mode in {'core', 'files', 'assets', 'async', 'window', 'gl', 'packets'}:
            forbidden = {'_USE_32BIT_TIME_T', 'DISABLE_SIGNATURE_CHECKS'}
            if target_os != 'WINDOWS':
                forbidden |= {'WIN32', '_WINDOWS', 'NOMINMAX', 'WIN32_LEAN_AND_MEAN',
                              '_CRT_SECURE_NO_WARNINGS'}
            if {d.split('=')[0] for d in definitions} & forbidden:
                errors.append(f"{target['name']}: Windows/client definitions leaked into core")
            for fragment in target.get('link', {}).get('commandFragments', []):
                if ((target_os != 'WINDOWS' and '.lib' in fragment['fragment'])
                        or '/ENTRY:' in fragment['fragment']):
                    errors.append(f"{target['name']}: Windows link flags in core")
        if mode in {'files', 'assets', 'async', 'window', 'gl'} and target['name'] in {'WowNativeFiles', 'FileServicesCheck'}:
            expected_sources = (
                {'Engine/Source/BlizzardCore/Portable/File.cpp',
                 'Engine/Source/BlizzardCore/Portable/FilePaths.cpp',
                 'Engine/Source/Os/Portable/File.cpp',
                 'Engine/Source/BlizzardCore/Portable/' + ('FileWindows.cpp' if target_os == 'WINDOWS' else 'FilePosix.cpp')}
                if target['name'] == 'WowNativeFiles' else
                {'Tools/PortableFiles/files.cpp'} | (
                    {'Tools/PortableFiles/interrupted.cpp'} if target_os == 'LINUX' else
                    {'Tools/PortableFiles/sparse_windows.cpp'} if target_os == 'WINDOWS' else set()))
            # CMake may return absolute paths or paths relative to the source root.
            if (len(sources) != len(expected_sources)
                    or any(not any(s.endswith(p) for s in sources) for p in expected_sources)):
                errors.append(f"{target['name']}: unexpected native file sources: {sources}")
            if 'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions:
                errors.append(f"{target['name']}: native file runtime ABI missing")
        if mode in {'assets', 'async', 'window', 'gl'} and target['name'] in {'WowAssetCodecs', 'WowNativeAssets', 'PackageServicesCheck', 'AssetDecodersCheck', 'NativeAssetCheck'}:
            allowed = {
                'WowAssetCodecs': ['ThirdParty/zlib/' + name + '.c' for name in
                    ['adler32', 'crc32', 'inffast', 'inflate', 'inftrees', 'zutil']] +
                    ['ThirdParty/zlib/contrib/blast/blast.c'] +
                    ['ThirdParty/bzip2/' + name + '.c' for name in
                     ['blocksort', 'bzlib', 'compress', 'crctable', 'decompress', 'huffman', 'randtable']] +
                    ['ThirdParty/lzma/LzmaDecode.c'],
                'WowNativeAssets': ['Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQEncrypt.cpp'] +
                    ['Engine/Source/BlizzardCore/Portable/' + name + '.cpp' for name in
                     ['PackageArchive', 'PackageCodecs', 'PackageSignatures', 'Packages',
                      'AssetTables', 'AssetTexture', 'AssetWorld']] + ['Engine/Source/Os/Portable/ModelArena.cpp'],
                'PackageServicesCheck': ['Tools/PortableAssets/packages.cpp'],
                'AssetDecodersCheck': ['Tools/PortableAssets/decoders.cpp'],
                'NativeAssetCheck': ['Tools/NativeAssets/main.cpp'],
            }[target['name']]
            if len(sources) != len(allowed) or any(not any(s.endswith(p) for s in sources) for p in allowed):
                errors.append(f"{target['name']}: unexpected asset sources: {sources}")
            if 'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions:
                errors.append(f"{target['name']}: native asset runtime ABI missing")
        if mode in {'async', 'window', 'gl'} and target['name'] in {'WowNativeAsync', 'AsyncFileCheck'}:
            path = ('Engine/Source/Services/Portable/AsyncFile.cpp' if target['name'] == 'WowNativeAsync'
                    else 'Tools/PortableAsync/async.cpp')
            if len(sources) != 1 or not sources[0].endswith(path) or 'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions:
                errors.append(f"{target['name']}: unexpected asynchronous file provider/ABI")
        if mode in {'window', 'gl'} and target['name'] in {'SDL3-static', 'SDL_uclibc'}:
            if any('/_deps/sdl3-src/' not in '/' + source and '/_deps/sdl3-build/' not in '/' + source for source in sources):
                errors.append(f"{target['name']}: source outside pinned SDL dependency")
            if target_os != 'WINDOWS' and any('/windows/' in source.lower() for source in sources):
                errors.append(f"{target['name']}: Windows SDL provider on POSIX")
        if mode in {'window', 'gl'} and target['name'] == 'SDLDesktopControl':
            if len(sources) != 1 or not sources[0].endswith('Tools/PortableWindow/desktop_control.cpp'):
                errors.append('SDLDesktopControl: source outside the standalone control')
            if 'WOWDECOMP_PORTABLE_RUNTIME=1' in definitions or any(
                    d['id'] not in build_utilities and
                    d['id'].split('::')[0] not in {'SDL3-static', 'SDL_uclibc'}
                    for d in target.get('dependencies', [])):
                errors.append('SDLDesktopControl: engine dependency leaked into the SDL-only control')
        if mode in {'window', 'gl'} and target['name'] in {'WowNativeWindow', 'NativeWindowCheck', 'NativeShell', 'ShellSettingsCheck'}:
            allowed = {'WowNativeWindow': ['Engine/Source/Os/Portable/Window.cpp', 'Engine/Source/Services/Portable/ShellSettings.cpp'],
                       'NativeWindowCheck': ['Tools/PortableWindow/window.cpp'],
                       'NativeShell': ['Tools/NativeShell/main.cpp'],
                       'ShellSettingsCheck': ['Tools/PortableWindow/settings.cpp']}[target['name']]
            if len(sources) != len(allowed) or any(not any(s.endswith(p) for s in sources) for p in allowed) or 'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions:
                errors.append(f"{target['name']}: unexpected window provider/ABI")
        if mode == 'gl' and target['name'] in {'WowNativeGxu', 'LegacyGxuCheck'}:
            allowed = (['Engine/Source/Gxu/' + name + '.cpp' for name in [
                'Portable/LegacyFonts', 'Portable/FontTexture', 'Portable/FontMath', 'Gxu', 'GxuFontFace',
                'GxuFontFont', 'GxuFontMain', 'GxuFontString', 'GxuFontTextureCache',
                'GxuFontMiscClasses', 'GxuFontUtil', 'IGxuFontGlyph']] +
                ['Engine/Source/Base/' + name + '.cpp' for name in
                 ['Handle', 'Status', 'Coordinate', 'ConvertUTF']] +
                ['Engine/Source/Tempest/C3Vector.cpp', 'Engine/Source/Tempest/C4Vector.cpp']
                if target['name'] == 'WowNativeGxu' else ['Tools/NativeGx/gxu.cpp'])
            if (len(sources) != len(allowed) or
                    any(not any(s.endswith(p) for s in sources) for p in allowed) or
                    'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions):
                errors.append(f"{target['name']}: unexpected native Gxu provider/ABI")
        if mode == 'gl' and target['name'] == 'WowNativeFonts':
            allowed = ['Engine/Source/Gxu/Portable/FontFace.cpp'] + [
                'ThirdParty/freetype/src/' + name + '.c' for name in [
                    'base/ftsystem', 'base/ftinit', 'base/ftdebug', 'base/ftbase',
                    'raster/raster', 'sfnt/sfnt', 'smooth/smooth', 'truetype/truetype']]
            if (len(sources) != len(allowed) or
                    any(not any(s.endswith(p) for s in sources) for p in allowed) or
                    'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions):
                errors.append('WowNativeFonts: unexpected font provider/ABI')
        if mode == 'gl' and target['name'] == 'NativeFontsCheck':
            if len(sources) != 1 or not sources[0].endswith('Tools/NativeGx/fonts.cpp'):
                errors.append('NativeFontsCheck: unexpected font fixture')
        if mode == 'gl' and target['name'] in {'WowNativeGl', 'NativeGlCheck', 'ShaderPackageCheck', 'GlRenderTargetsCheck', 'GlTransformsCheck', 'GlTexturesCheck', 'GlBuffersCheck', 'LegacyGxCheck'}:
            allowed = {'WowNativeGl': ['Engine/Source/Gx/Portable/' + name + '.cpp'
                                      for name in ['GlContext', 'GlBuffers', 'GlProgram', 'GlRenderTargets', 'GlTransforms', 'ShaderPackage']] +
                                      ['Engine/Source/Gx/CGxMatrixStack.cpp',
                                       'Engine/Source/Tempest/C44Matrix.cpp',
                                       'Engine/Source/Tempest/C33Matrix.cpp'] + [
                                      'Engine/Source/Gx/Portable/LegacyDevice.cpp',
                                      'Engine/Source/Gx/Gx.cpp',
                                      'Engine/Source/Gx/Buffer.cpp',
                                      'Engine/Source/Gx/CGxDevice/CGxDevice.cpp',
                                      'Engine/Source/Gx/CGxDevice/CGxShader.cpp',
                                      'Engine/Source/Gx/CGxDevice/EmergencyMem.cpp',
                                      'Engine/Source/Gx/CGxTex.cpp',
                                      'Engine/Source/Gx/CGxTexFlags.cpp',
                                      'Engine/Source/Gx/CGxPool.cpp',
                                      'Engine/Source/Gx/CGxBuf.cpp',
                                      'Engine/Source/Gx/CGxFormat.cpp',
                                      'Engine/Source/Gx/CGxGammaRamp.cpp',
                                      'Engine/Source/Tempest/CiRect.cpp',
                                      'Engine/Source/Tempest/CRect.cpp',
                                      'Engine/Source/Gx/CGxLight.cpp',
                                      'Engine/Source/Tempest/C2Vector.cpp',
                                      'Engine/Source/Tempest/C4Plane.cpp',
                                      ],
                       'NativeGlCheck': ['Tools/NativeGx/main.cpp'],
                       'LegacyGxCheck': ['Tools/NativeGx/legacy.cpp'],
                       'GlRenderTargetsCheck': ['Tools/NativeGx/targets.cpp'],
                       'GlTransformsCheck': ['Tools/NativeGx/transforms.cpp'],
                       'GlTexturesCheck': ['Tools/NativeGx/textures.cpp'],
                       'GlBuffersCheck': ['Tools/NativeGx/buffers.cpp'],
                       'ShaderPackageCheck': ['Tools/NativeGx/shaders.cpp']}[target['name']]
            if (len(sources) != len(allowed) or
                    any(not any(s.endswith(p) for s in sources) for p in allowed) or
                    'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions):
                errors.append(f"{target['name']}: unexpected native Gx context provider/ABI")
        if mode == 'packets' and target['name'] in {'WowNativePackets', 'NativePacketCheck'}:
            allowed = {'WowNativePackets': ['Engine/Source/Base/CDataStore.cpp',
                       'Engine/Source/Storm/Source/SArc4.cpp',
                       'Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.cpp'],
                       'NativePacketCheck': ['Tools/NativeNet/packets.cpp']}[target['name']]
            if (len(sources) != len(allowed) or
                    any(not any(s.endswith(p) for s in sources) for p in allowed) or
                    'WOWDECOMP_PORTABLE_RUNTIME=1' not in definitions):
                errors.append(f"{target['name']}: unexpected native packet source/ABI")
        if mode == 'windows' and target['name'] in {'Wow', 'WowClient'}:
            if not any(s.endswith(f'/{expected_platform}/WinMain.cpp') for s in sources):
                errors.append(f"{target['name']}: selected Windows entry missing")
            if not any(s.endswith('waystone.rc') for s in sources):
                errors.append(f"{target['name']}: Windows resource missing")
    return errors


def read_targets(build_dir):
    reply = build_dir / '.cmake/api/v1/reply'
    indexes = sorted(reply.glob('index-*.json'))
    index = json.loads(indexes[-1].read_text())
    model_file = index['reply']['codemodel-v2']['jsonFile']
    model = json.loads((reply / model_file).read_text())
    configuration = next((c for c in model['configurations'] if c['name'] == 'Release'),
                         model['configurations'][0])
    return [json.loads((reply / t['jsonFile']).read_text()) for t in configuration['targets']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--mode', choices=['core', 'files', 'assets', 'async', 'window', 'gl', 'packets', 'windows'], required=True)
    parser.add_argument('--os', required=True)
    parser.add_argument('--cpu', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    targets = read_targets(args.build_dir)
    errors = audit(targets, args.mode, args.os, args.cpu)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'mode': args.mode, 'os': args.os, 'cpu': args.cpu,
                                     'errors': errors, 'targets': targets}, indent=2) + '\n')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'Build graph passed: {args.mode}, {args.os}/{args.cpu}')


if __name__ == '__main__':
    main()
