"""Redistributable authored XML/TOC graph; never reads original game data."""
import hashlib
import json
from pathlib import Path
import sys

FILES = {
    'Fixture.toc': b'# authored native interface\r\nmain.lua\r\nPanels\\Main.xml\r\nmain.lua\r\n',
    'main.lua': b'fixtureCount=(fixtureCount or 0)+1\n',
    'Panels/Main.xml': b'<Ui><Include file="..\\Shared.xml"/><Script file="..\\scripts\\nested.lua"/><Script>fixtureInline=true</Script></Ui>',
    'Shared.xml': b'<Ui><Script file="scripts\\nested.lua" /></Ui>',
    'scripts/nested.lua': b'fixtureNested=true\n',
    'Bindings.xml': b'<Bindings/>',
    'Missing.toc': b'absent.lua\r\n',
}
# Independent explicit expected ordering: retain duplicates, depth first, then
# append the extra file raw. This is not an implementation of the C++ traversal.
ORDER = ['Fixture.toc', 'main.lua', 'Panels/Main.xml', 'Shared.xml',
         'scripts/nested.lua', 'scripts/nested.lua', 'main.lua', 'Bindings.xml']

def main(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, data in FILES.items():
        path = directory/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    fixture = json.loads(Path(__file__).with_name('signature_fixture.json').read_text())
    for name in ['Fixture.toc.sig', 'Missing.toc.sig', 'modulus']:
        (directory/(name if name.endswith('.sig') else 'modulus.bin')).write_bytes(bytes.fromhex(fixture[name]))
    (directory/'expected.bin').write_bytes(hashlib.md5(b''.join(FILES[n] for n in ORDER)).digest())

if __name__ == '__main__':
    main(Path(sys.argv[1]))
