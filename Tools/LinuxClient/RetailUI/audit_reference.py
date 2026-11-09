#!/usr/bin/env python3
"""Audit user-supplied Forever UI; emit identities/dependencies, never source.

This is a static declared-load graph, not a Lua evaluator or a claim that every
runtime global is available. Run outside CI; see docs/retail/COMPATIBILITY.md.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
PIN = '9dc84f0d3bbae1768ac3405b3716000bd93eda06'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def selected(line):
    """Camelot game, Mainline family; preserve other tags in the report."""
    tags = {'camelot', 'mainline'}
    for values in re.findall(r'\[AllowLoad\s+([^]]+)\]', line):
        if not {'game', 'both'}.intersection(re.split(r'[,\s]+', values.strip().lower())):
            return False
    for mode, values in re.findall(r'\[(AllowLoadGameType|ExcludeLoadGameType)\s+([^]]+)\]', line):
        matches = bool(tags.intersection(re.split(r'[,\s]+', values.strip().lower())))
        if (mode == 'AllowLoadGameType' and not matches) or (mode == 'ExcludeLoadGameType' and matches):
            return False
    return True


def audit(root):
    root = root.resolve()
    addon_root = root / 'Interface/AddOns'
    files, edges, missing, cycles, optional = {}, [], set(), [], []
    visiting, visited = [], set()

    def resolve(base, name):
        name = name.replace('\\', '/').replace('[Game]', 'Camelot').replace('[Family]', 'Mainline')
        path = ((root / name) if name.lower().startswith('interface/') else (base / name)).resolve()
        if not path.is_relative_to(root):
            raise ValueError('Reference path escapes supplied root: ' + name)
        return path

    def key(path):
        return path.relative_to(root).as_posix()

    def visit(path, parent=None, kind='file'):
        name = key(path)
        if parent:
            edges.append({'from': parent, 'to': name, 'kind': kind})
        if name in visiting:
            cycles.append(visiting[visiting.index(name):] + [name])
            return
        if name in visited:
            return
        visited.add(name)
        if not path.is_file():
            missing.add(name)
            return
        visiting.append(name)
        text = path.read_text(encoding='utf-8-sig')
        files[name] = {'sha256': sha(path), 'bytes': path.stat().st_size}
        if path.suffix == '.toc':
            for line in text.splitlines():
                line = line.strip()
                if not line or not selected(line):
                    continue
                plain = re.sub(r'\s*\[(?:AllowLoad\w*|ExcludeLoad\w*)\s+[^]]+\]', '', line).strip()
                if plain.startswith('##'):
                    meta = re.match(r'##\s*(Dependencies|RequiredDeps?|Dep|OptionalDeps?):\s*(.*)', plain, re.I)
                    if meta:
                        for dep in filter(None, re.split(r'[,\s]+', meta[2])):
                            dest = addon_root / dep / (dep + '.toc')
                            if meta[1].lower().startswith('optional'):
                                optional.append({'from': name, 'to': key(dest), 'kind': 'optional-addon'})
                            else:
                                visit(dest, name, 'required-addon')
                elif not plain.startswith('#'):
                    visit(resolve(path.parent, plain), name, 'toc-entry')
        elif path.suffix == '.xml':
            tree = ET.fromstring(text)
            for element in tree.iter():
                tag = element.tag.rsplit('}', 1)[-1]
                if tag in {'Include', 'Script'} and 'file' in element.attrib:
                    visit(resolve(path.parent, element.attrib['file']), name, 'xml-' + tag.lower())
        visiting.pop()

    visit(addon_root / 'Blizzard_EditMode/Blizzard_EditMode.toc')
    # Index all exported definitions for references whose provider is implicit in
    # bootstrap/gameplay loading rather than declared by the Edit Mode TOC.
    providers = {}
    for path in sorted(addon_root.rglob('*.xml')):
        try:
            elements = ET.parse(path).iter()
            for element in elements:
                if 'name' in element.attrib and element.attrib.get('virtual') == 'true':
                    providers.setdefault(element.attrib['name'], []).append(key(path))
        except ET.ParseError:
            continue
    templates, mixins, apis, methods, resources = {}, {}, {}, {}, {}
    for name in sorted(files):
        text = (root / name).read_text(encoding='utf-8-sig')
        if name.endswith('.xml'):
            for element in ET.fromstring(text).iter():
                for token in filter(None, re.split(r'[,\s]+', element.get('inherits', ''))):
                    templates.setdefault(token, {'used_by': [], 'providers': providers.get(token, [])})['used_by'].append(name)
                for token in filter(None, re.split(r'[,\s]+', element.get('mixin', ''))):
                    mixins.setdefault(token, []).append(name)
                for attribute in ['atlas', 'file']:
                    if attribute in element.attrib and element.tag.rsplit('}', 1)[-1] not in {'Script', 'Include'}:
                        resources.setdefault(element.attrib[attribute], []).append(name)
        if name.endswith('.lua'):
            # Conservative lexical inventories, not resolved call/dependency graphs.
            for token in set(re.findall(r'\b(C_\w+\.\w+)\s*\(', text)):
                apis.setdefault(token, []).append(name)
            for token in set(re.findall(r':(\w+)\s*\(', text)):
                methods.setdefault(token, []).append(name)
    provider_inputs = {name: sha(root / name) for entry in templates.values() for name in entry['providers']}
    identities = ''.join('file\0' + name + '\0' + files[name]['sha256'] + '\n' for name in sorted(files))
    identities += ''.join('provider\0' + name + '\0' + provider_inputs[name] + '\n' for name in sorted(provider_inputs))
    identities += ''.join('template\0' + name + '\0' + '\0'.join(templates[name]['providers']) + '\n' for name in sorted(templates))
    fingerprint = hashlib.sha256(identities.encode()).hexdigest()
    return {'schema': 1, 'commit': PIN, 'game': 'Camelot', 'family': 'Mainline',
            'scope': 'required TOCs and XML file edges; optional edges listed; Lua inventories are lexical, runtime dependencies unresolved',
            'fingerprint': fingerprint, 'files': files, 'edges': edges,
            'optional_addons': optional, 'missing_files': sorted(missing), 'cycles': cycles,
            'templates': templates, 'provider_inputs': provider_inputs, 'mixins': mixins, 'namespaced_calls': apis,
            'method_calls': methods, 'resource_references': resources}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='external source root containing Interface/AddOns')
    parser.add_argument('--report', type=Path, help='write metadata-only graph outside the repository')
    parser.add_argument('--binary', type=Path, help='hash-verified hosted RovRetailXMLCheck')
    parser.add_argument('--binary-sha256', help='expected hosted executable hash')
    args = parser.parse_args()
    report = audit(args.source)
    lock = json.loads((HERE / 'references.json').read_text())
    expected = lock['forever']['declared_graph_sha256']
    if report['fingerprint'] != expected:
        raise SystemExit('Supplied source differs from the pinned declared graph; no reference execution performed')
    print(json.dumps({k: report[k] for k in ['commit', 'game', 'family', 'fingerprint', 'missing_files', 'cycles']}, indent=2))
    print('declared-files=' + str(len(report['files'])) + ' namespace-calls=' + str(len(report['namespaced_calls'])))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    if args.binary:
        if not args.binary_sha256 or sha(args.binary) != args.binary_sha256:
            raise SystemExit('Pass the matching hosted binary SHA256 before reference execution')
        script = args.source / 'Interface/AddOns/Blizzard_EditMode/Camelot/EditModeUtil.lua'
        subprocess.run([str(args.binary.resolve()), '--reference', str(script.resolve())], check=True)
    elif args.binary_sha256:
        parser.error('--binary-sha256 requires --binary')


if __name__ == '__main__':
    main()
