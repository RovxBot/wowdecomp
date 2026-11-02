"""Assemble a relocatable desktop archive from an Actions-built DXVK client."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile


# The host owns glibc/its loader and GPU ICDs. Ship the Vulkan loader and all
# other linked dependencies; never copy a runner's software/hardware GPU driver.
HOST_LIBRARIES = {'libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0',
                  'librt.so.1', 'libresolv.so.2', 'libutil.so.1', 'ld-linux-x86-64.so.2'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linked_libraries(binary, environment):
    result = subprocess.check_output(['ldd', str(binary)], env=environment, text=True)
    if 'not found' in result:
        raise RuntimeError('Unresolved runtime dependency for ' + str(binary) + ':\n' + result)
    libraries = {}
    for line in result.splitlines():
        match = re.fullmatch(r'\s*(\S+) => (/.+) \(0x[0-9a-fA-F]+\)', line)
        if match:
            libraries[match[1]] = Path(match[2])
    return libraries


def distribution_notice(source, notices):
    path = source.resolve()
    # Ubuntu's merged-/usr aliases differ between dpkg's database and ldd.
    candidates = [str(path), str(source)]
    if str(path).startswith('/usr/'):
        candidates.append(str(path)[4:])
    for candidate in candidates:
        result = subprocess.run(['dpkg-query', '-S', candidate], text=True, capture_output=True)
        if result.returncode == 0:
            package = result.stdout.split(': ', 1)[0].splitlines()[0]
            break
    else:
        raise RuntimeError('Cannot attribute bundled distribution library: ' + str(source))
    version = subprocess.check_output(['dpkg-query', '-W', '-f=${Version}', package], text=True).strip()
    name = package.split(':', 1)[0]
    copyright_file = Path('/usr/share/doc') / name / 'copyright'
    target = notices / (name + '-copyright')
    shutil.copy2(copyright_file, target)
    return dict(package=package, version=version, notice='notices/' + target.name)


def bundle_runtime(output, environment):
    """Resolve the real ELF closure, including DXVK's dynamically opened loader."""
    library_dir = output / 'lib'
    fallback_dir = output / 'runtime'
    fallback_dir.mkdir(exist_ok=True)
    notices = output / 'notices'
    pending = [output / 'WowNativeClient', *sorted(library_dir.glob('*'))]
    loader_lines = subprocess.check_output(['ldconfig', '-p'], text=True).splitlines()
    loader = [Path(line.split(' => ')[1]) for line in loader_lines
              if re.match(r'\s*libvulkan\.so\.1 \(.*x86-64', line)]
    if len(loader) != 1:
        raise RuntimeError('Expected one system x86-64 Vulkan loader')
    pending += loader
    records = {}
    visited = set()
    while pending:
        binary = pending.pop()
        if binary.resolve() in visited:
            continue
        visited.add(binary.resolve())
        dependencies = linked_libraries(binary, environment)
        if binary in loader:
            dependencies['libvulkan.so.1'] = binary
        for soname, source in dependencies.items():
            if soname in HOST_LIBRARIES:
                continue
            destination = library_dir / soname
            if not destination.exists():
                destination = fallback_dir / soname
            if destination.exists():
                if digest(source) != digest(destination):
                    raise RuntimeError('Conflicting runtime SONAME: ' + soname)
            else:
                record = distribution_notice(source, notices)
                shutil.copy2(source, destination)
                record.update(sha256=digest(destination), source=str(source.resolve()))
                records[soname] = record
            pending.append(source)
    return dict(host_libraries=sorted(HOST_LIBRARIES), bundled=records,
                minimum_glibc='2.39', python='3.10+', display='X11 or Xwayland',
                library_policy='Pinned SDL/DXVK/ASan in lib; host distribution libraries before bundled runtime fallbacks',
                gpu='Host Vulkan 1.3 capable GPU/ICD; no GPU driver bundled')


def desktop_entry(test_build=False, activate=False):
    # %k is the desktop file's own location, supplied as one argument by the
    # desktop. No shell interpolation: spaces, quotes, dollars and Unicode work.
    bootstrap = ("import pathlib,runpy,sys,urllib.parse; "
                 "s=sys.argv.pop(1); "
                 "p=pathlib.Path(urllib.parse.unquote(urllib.parse.urlparse(s).path) if s.startswith('file:') else s).resolve().parent; "
                 "sys.path.insert(0,str(p)); runpy.run_path(str(p/'launch.py'),run_name='__main__')")
    name = 'WoW Native'
    if test_build:
        name += ' TEST-SERVER' + (' login (auth/realm lists only)' if activate else ' build (login disabled)')
    return ('[Desktop Entry]\nType=Application\nVersion=1.0\nName=' + name +
            '\nComment=Launch the native 3.3.5a Linux client with original data\n'
            'Icon=applications-games\nTerminal=true\nCategories=Game;\n'
            'Exec=python3 -I -c "' + bootstrap + '" %k --desktop' +
            (' --test-server-login' if activate else '') + '\n')


def stage_launchers(output, root, test_build):
    if not test_build:
        for name in ['Launch TEST-SERVER.sh', 'Play WoW TEST-SERVER.desktop']:
            (output / name).unlink(missing_ok=True)
    for source, name in [(root / 'Tools/NativeD3D9/launch.py', 'launch.py'),
                         (root / 'Tools/Porting/run_native_client.py', 'run_native_client.py'),
                         (root / 'Tools/NativeD3D9/LINUX_README.txt', 'README-Linux.txt')]:
        shutil.copy2(source, output / name)
    shell = ('#!/bin/sh\nset -eu\numask 077\n'
             'package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
             'if ! command -v python3 >/dev/null 2>&1; then\n'
             '  printf "%s\\n" "WoW Native requires Python 3.10 or newer. Install python3 and try again." >&2\n'
             '  if [ -t 0 ]; then read -r reply; fi\n'
             '  exit 1\nfi\n'
             'exec python3 "$package_dir/launch.py" --desktop "$@"\n')
    scripts = [('Launch WoW.sh', False), ('Play WoW.desktop', False)]
    if test_build:
        scripts += [('Launch TEST-SERVER.sh', True), ('Play WoW TEST-SERVER.desktop', True)]
    for name, activate in scripts:
        path = output / name
        if name.endswith('.desktop'):
            content = desktop_entry(test_build, activate)
        else:
            content = shell.replace('--desktop "$@"', '--desktop --test-server-login "$@"') if activate else shell
        path.write_text(content)
        path.chmod(0o755)
    (output / 'WowNativeClient').chmod(0o755)


def desktop_archive(output, root):
    manifest = json.loads((output / 'manifest.json').read_text())
    test_build = manifest['test_server_login']
    strict = manifest['preset'].endswith('-asan')
    name = 'WoW-Native-Linux' + ('-TEST-SERVER' if test_build else '') + ('-strict' if strict else '')
    destination = root / 'artifacts' / ('linux-desktop-d3d9' + ('-TEST-SERVER' if test_build else '') + ('-asan' if strict else ''))
    destination.mkdir(parents=True, exist_ok=True)
    archive_path = destination / (name + '.tar.gz')
    with tempfile.TemporaryDirectory() as temp:
        package = Path(temp) / name
        package.mkdir()
        for entry in ['WowNativeClient', 'lib', 'runtime', 'notices', 'launch.py', 'run_native_client.py',
                      'README-Linux.txt', 'Launch WoW.sh', 'Play WoW.desktop'] + (
                          ['Launch TEST-SERVER.sh', 'Play WoW TEST-SERVER.desktop'] if test_build else []):
            source = output / entry
            if source.is_dir():
                shutil.copytree(source, package / entry)
            else:
                shutil.copy2(source, package / entry)
        manifest['files'] = {p.relative_to(package).as_posix(): digest(p)
                             for p in sorted(package.rglob('*')) if p.is_file()}
        (package / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        for desktop in package.glob('*.desktop'):
            subprocess.run(['desktop-file-validate', str(desktop)], check=True)
        with tarfile.open(archive_path, 'w:gz') as archive:
            archive.add(package, arcname=name)
    (destination / (archive_path.name + '.sha256')).write_text(digest(archive_path) + '  ' + archive_path.name + '\n')
    return archive_path
