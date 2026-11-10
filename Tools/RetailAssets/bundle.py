#!/usr/bin/env python3
"""Stage/verify private Alpha 0.2.0 bootstrap inputs; never modifies game data."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import xml.etree.ElementTree as ET

from casc import Storage, sha256

REPO = Path(__file__).resolve().parents[2]
VERSION = '1.60.1.70862'
SCOPE = 'forever-bootstrap-enUS-v1'
FONT_XML = 'Interface/AddOns/Blizzard_Fonts_Shared/Mainline/Fonts.xml'


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def external(path, install=None):
    path = Path(path).resolve()
    if path.is_relative_to(REPO) or (install and (
            path.is_relative_to(install) or install.is_relative_to(path))):
        raise ValueError('Output must be outside Git and the source installation')
    return path


def safe_file(root, name):
    logical = PurePosixPath(name)
    if not name or '\\' in name or logical.is_absolute() or '..' in logical.parts:
        raise ValueError('Unsafe bundle path: ' + name)
    path = root / logical
    if path.resolve() != path.absolute() or not path.is_file():
        raise ValueError('Missing, non-regular or symlinked input: ' + name)
    return path


def installation(root):
    build_info = root / '.build.info'
    lines = build_info.read_text().splitlines()
    fields = [value.split('!', 1)[0] for value in lines[0].split('|')]
    rows = [dict(zip(fields, row.split('|'))) for row in lines[1:] if row]
    active = [row for row in rows if row.get('Active') == '1' and row.get('Product') == 'wowf']
    if len(active) != 1 or active[0].get('Version') != VERSION:
        raise ValueError('Require the selected active wowf ' + VERSION + ' installation')
    row = active[0]
    if 'enUS' not in row.get('Tags', '').split():
        raise ValueError('This bootstrap bundle requires installed enUS data')
    configs = {}
    for key in ('Build Key', 'CDN Key'):
        value = row[key]
        if len(value) != 32 or any(c not in '0123456789abcdef' for c in value):
            raise ValueError('Invalid storage key')
        path = root / 'Data/config' / value[:2] / value[2:4] / value
        configs[key] = {'key': value, 'sha256': sha256(path)}
    # Inventory metadata, not account/session contents or a 50GB archive rehash.
    inventory = [(p.relative_to(root).as_posix(), p.stat().st_size, p.stat().st_mtime_ns)
                 for p in sorted((root / 'Data').rglob('*')) if p.is_file()]
    return {'product': 'wowf', 'version': VERSION, 'locale': 'enUS',
            'build_info_sha256': sha256(build_info), 'configs': configs,
            'storage_files': len(inventory),
            'storage_metadata_sha256': hashlib.sha256(json.dumps(inventory).encode()).hexdigest()}


def reference_plan(root):
    spec = importlib.util.spec_from_file_location('retail_audit', REPO / 'Tools/LinuxClient/RetailUI/audit_reference.py')
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    lock = json.loads((REPO / 'Tools/LinuxClient/RetailUI/references.json').read_text())['forever']
    graph = audit.audit(root)
    if graph['fingerprint'] != lock['declared_graph_sha256'] or graph['missing_files'] or graph['cycles']:
        raise ValueError('Forever reference graph does not match the selected pin')
    files = {name: entry['sha256'] for name, entry in graph['files'].items()}
    files.update(graph['provider_inputs'])
    files.update(lock['resource_evidence'])
    for name, expected in files.items():
        if sha256(safe_file(root, name)) != expected:
            raise ValueError('Reference file checksum mismatch: ' + name)
    return lock, files, set(graph['files']) | set(graph['provider_inputs'])


def roman_fonts(path):
    names = set()

    def visit(element):
        if element.tag.rsplit('}', 1)[-1] == 'Member' and element.get('alphabet') != 'roman':
            return
        if 'font' in element.attrib:
            name = element.attrib['font'].replace('\\', '/')
            if not name.startswith('Fonts/') or '..' in PurePosixPath(name).parts:
                raise ValueError('Unexpected font path')
            names.add(name)
        for child in element:
            visit(child)

    visit(ET.parse(path).getroot())
    if not names:
        raise ValueError('No roman fonts in selected source')
    return sorted(names)


def stage(args):
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        raise ValueError('Private game assets must be staged outside ordinary CI')
    install = args.install.resolve()
    output = external(args.output, install)
    if output.exists():
        raise ValueError('Output already exists')
    before = installation(install)
    lock, references, local_ui = reference_plan(args.ui_root.resolve())
    fonts = roman_fonts(args.ui_root / FONT_XML)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Failed staging retains no manifest usable by verify/prepare-release.
    with tempfile.TemporaryDirectory(prefix='.forever-staging-', dir=output.parent) as temporary:
        staging = Path(temporary)
        records = {}
        for name, expected in sorted(references.items()):
            target = staging / 'reference' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(safe_file(args.ui_root.resolve(), name), target)
            if sha256(target) != expected:
                raise ValueError('Reference changed during staging')
            records['reference/' + name] = {'sha256': expected, 'bytes': target.stat().st_size,
                                            'origin': 'pinned-ui-export'}
        storage = Storage(args.reader, args.reader_sha256, install, before['configs']['Build Key']['key'])
        try:
            for name in sorted(local_ui):
                data, identity = storage.read(name)
                reference = (args.ui_root / name).read_bytes()
                if data.replace(b'\r\n', b'\n') != reference.replace(b'\r\n', b'\n'):
                    raise ValueError('Installed UI differs from selected export beyond CRLF: ' + name)
                target = staging / 'local-ui' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                records['local-ui/' + name] = dict(identity, sha256=sha256(target), bytes=len(data),
                    origin='local-offline-casc', logical_path=name, reference_sha256=references[name],
                    reference_comparison='exact' if data == reference else 'CRLF-to-LF-only')
            print('Staged installed UI files:', len(local_ui), flush=True)
            for name in fonts:
                data, identity = storage.read(name)
                target = staging / 'assets' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                records['assets/' + name] = dict(identity, sha256=sha256(target), bytes=len(data),
                                                origin='local-offline-casc', logical_path=name)
                print('Staged', name, len(data), flush=True)
        finally:
            storage.close()
        after = installation(install)
        if before != after:
            raise ValueError('Source installation metadata changed during extraction')
        manifest = {'schema': 1, 'scope': SCOPE, 'target_release': '0.2.0',
                    'installation': before, 'reference_commit': lock['commit'],
                    'reference_graph': lock['declared_graph_sha256'],
                    'reader_sha256': args.reader_sha256, 'files': records,
                    'limits': ['Bootstrap UI/font inputs only; runtime dependencies are not a complete Edit Mode closure',
                               'No runtime CASC mount or automatic application activation',
                               'Native slug font support, atlases/textures and gameplay integration remain unqualified']}
        write_json(staging / 'manifest.json', manifest)
        verify(staging, sha256(staging / 'manifest.json'))
        os.replace(staging, output)
    print('Bundle:', output, '\nManifest SHA256:', sha256(output / 'manifest.json'))


def verify(root, expected):
    root = root.resolve()
    path = safe_file(root, 'manifest.json')
    if sha256(path) != expected:
        raise ValueError('Bundle manifest checksum mismatch')
    manifest = json.loads(path.read_text())
    if (manifest['schema'], manifest['scope'], manifest['target_release']) != (1, SCOPE, '0.2.0'):
        raise ValueError('Unsupported release bundle')
    if (manifest['installation']['product'], manifest['installation']['version'],
            manifest['installation']['locale']) != ('wowf', VERSION, 'enUS'):
        raise ValueError('Unexpected bundle product/version/locale')
    files = manifest['files']
    required = {'reference/' + FONT_XML, 'assets/Fonts/FRIZQT__.TTF',
                'assets/Fonts/MORPHEUS.ttf', 'assets/Fonts/ARIALN.TTF'}
    if not required.issubset(files):
        raise ValueError('Required bootstrap resources absent')
    for name, record in files.items():
        source = safe_file(root, name)
        if source.stat().st_size != record['bytes'] or sha256(source) != record['sha256']:
            raise ValueError('Bundle asset checksum mismatch: ' + name)
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() or p.is_symlink()}
    if actual != set(files) | {'manifest.json'}:
        raise ValueError('Unlisted bundle entries')
    return manifest


def prepare_release(args):
    """Private assembly gate, after Actions compilation, before final packaging."""
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        raise ValueError('Private game assets must be assembled outside ordinary CI')
    bundle = external(args.bundle)
    manifest = verify(bundle, args.manifest_sha256)
    client = args.client.resolve()
    if not client.is_file() or sha256(client) != args.client_sha256:
        raise ValueError('Client artifact checksum mismatch')
    output = external(args.output)
    if output.exists() or output.is_relative_to(bundle) or bundle.is_relative_to(output):
        raise ValueError('Release output must be new and separate from the bundle')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.alpha-assembly-', dir=output.parent) as temporary:
        staging = Path(temporary)
        shutil.copytree(bundle, staging / 'forever-bootstrap')
        verify(staging / 'forever-bootstrap', args.manifest_sha256)
        (staging / 'client').mkdir()
        shutil.copyfile(client, staging / 'client' / client.name)
        if sha256(staging / 'client' / client.name) != args.client_sha256:
            raise ValueError('Client changed during assembly')
        write_json(staging / 'release-inputs.json', {
            'schema': 1, 'release': '0.2.0', 'status': 'inputs-prepared-runtime-acceptance-pending',
            'client': {'name': client.name, 'sha256': args.client_sha256},
            'bundle_manifest_sha256': args.manifest_sha256, 'scope': manifest['scope']})
        os.replace(staging, output)
    print('Verified private release inputs:', output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    stage_parser = commands.add_parser('stage')
    for name in ('install', 'ui-root', 'reader', 'output'):
        stage_parser.add_argument('--' + name, type=Path, required=True)
    stage_parser.add_argument('--reader-sha256', required=True)
    for command in ('verify', 'prepare-release'):
        cmd = commands.add_parser(command)
        cmd.add_argument('--bundle', type=Path, required=True)
        cmd.add_argument('--manifest-sha256', required=True)
        if command == 'prepare-release':
            cmd.add_argument('--client', type=Path, required=True)
            cmd.add_argument('--client-sha256', required=True)
            cmd.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'stage':
        stage(args)
    elif args.command == 'verify':
        result = verify(args.bundle, args.manifest_sha256)
        print('Verified', len(result['files']), 'files for', result['scope'])
    else:
        prepare_release(args)


if __name__ == '__main__':
    main()
