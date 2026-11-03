"""Package the Actions-built full client as a hash-verified type-2 AppImage."""
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from prepare import HERE, LOCK, digest, download


def package(source, binary, output, metadata):
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise RuntimeError('Binary packaging/qualification runs in GitHub Actions')
    app = output / 'AppDir'
    app.mkdir()
    for name in ['lib', 'runtime', 'notices']:
        (app / name).mkdir()
    shutil.copy2(binary, app / 'WowClient64')
    shutil.copy2(HERE / 'launch.py', app / 'launch.py')
    shutil.copy2(source / 'source-audit.json', app / 'source-audit.json')
    shutil.copy2(HERE / 'upstream.json', app / 'upstream.json')
    # Reuse the existing reviewed ELF dependency and Debian notice attribution.
    spec = importlib.util.spec_from_file_location('dxvk_packaging', HERE.parent / 'NativeD3D9/package.py')
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    pending = [app / 'WowClient64']
    loader = subprocess.check_output(['ldconfig', '-p'], text=True)
    vulkan = [Path(line.split(' => ')[1]) for line in loader.splitlines()
              if re.match(r'\s*libvulkan\.so\.1 \(.*x86-64', line)]
    if len(vulkan) != 1:
        raise RuntimeError('Expected one x86-64 Vulkan loader')
    pending += vulkan
    visited, dependencies = set(), {}
    while pending:
        path = pending.pop()
        if path.resolve() in visited:
            continue
        visited.add(path.resolve())
        libraries = helpers.linked_libraries(path, os.environ)
        if path in vulkan:
            libraries['libvulkan.so.1'] = path
        for name, library in libraries.items():
            if name in helpers.HOST_LIBRARIES:
                continue
            # SDL must match the compiled client. Other libraries are fallbacks.
            directory = 'lib' if name.startswith('libSDL3') else 'runtime'
            target = app / directory / name
            if target.exists():
                if digest(library) != digest(target):
                    raise RuntimeError('Conflicting SONAME: ' + name)
            else:
                record = helpers.distribution_notice(library, app / 'notices')
                shutil.copy2(library, target)
                dependencies[name] = dict(record, sha256=digest(target))
            pending.append(library)
    for path in source.rglob('*'):
        if path.is_file() and re.match(r'(?i)^(licen[cs]e|copying|notice|copyright)([._-].*|s)?$', path.name):
            target = app / 'notices/upstream' / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    (app / 'notices/CREDITS.txt').write_text(
        '3.3.5a client reconstruction: SatyPardus/wowdecomp and upstream contributors.\n'
        'Full Linux platform, Vulkan and gameplay integration: RosemyneH/wowdecomp, pinned in upstream.json.\n'
        'RovxBot adaptations: source-audit.json and Tools/LinuxClient in RovxBot/wowdecomp.\n'
        'FMOD Sound System, copyright Firelight Technologies Pty, Ltd., 1994-2009.\n'
        'FMOD non-commercial licence and codec notices are retained below upstream/.\n'
        'AppImage runtime: AppImage/type2-runtime, MIT; see type2-runtime-LICENSE.\n')
    # The exact runtime licence is pinned by its immutable source commit.
    licence = app / 'notices/type2-runtime-LICENSE'
    with __import__('urllib.request', fromlist=['urlopen']).urlopen(
            'https://raw.githubusercontent.com/AppImage/type2-runtime/' + LOCK['appimage_runtime']['commit'] + '/LICENSE', timeout=60) as response:
        licence.write_bytes(response.read())
    (app / 'wowdecomp.desktop').write_text('[Desktop Entry]\nType=Application\nName=WoW Native Linux\nExec=AppRun\nIcon=wowdecomp\nTerminal=true\nCategories=Game;\n')
    (app / 'wowdecomp.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" rx="12" fill="#182536"/><text x="32" y="45" text-anchor="middle" font-family="serif" font-size="45" fill="#d9b85c">W</text></svg>\n')
    (app / 'AppRun').write_text('#!/bin/sh\nset -eu\n'
                              'app_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                              'exec python3 -I -B "$app_dir/launch.py" "$@"\n')
    (app / 'AppRun').chmod(0o755)
    (app / 'WowClient64').chmod(0o755)
    metadata.update(dependencies=dependencies, minimum_glibc='2.43', python='3.10+',
                    host_libraries=sorted(helpers.HOST_LIBRARIES),
                    files={p.relative_to(app).as_posix(): digest(p) for p in sorted(app.rglob('*')) if p.is_file()})
    (app / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    runtime = output / 'runtime-x86_64'
    download(LOCK['appimage_runtime']['url'], runtime, LOCK['appimage_runtime']['sha256'])
    filesystem = output / 'application.squashfs'
    subprocess.run(['mksquashfs', str(app), str(filesystem), '-noappend', '-comp', 'gzip', '-all-root', '-no-xattrs'], check=True)
    name = 'WoW-Linux-x86_64' + ('-TEST-SERVER' if metadata['test_server_login'] else '') + ('-strict' if metadata['strict'] else '') + '.AppImage'
    application = output / name
    with application.open('wb') as stream:
        for path in [runtime, filesystem]:
            with path.open('rb') as contents:
                shutil.copyfileobj(contents, stream)
    application.chmod(0o755)
    (output / (name + '.sha256')).write_text(digest(application) + '  ' + name + '\n')
    shutil.copy2(app / 'manifest.json', output / 'manifest.json')
    return application
