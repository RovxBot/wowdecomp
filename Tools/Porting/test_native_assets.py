"""Source/fixture identity checks; no client/dependency compilation."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativeAssetPolicy(unittest.TestCase):
    def test_original_signature_public_key_and_word_order(self):
        def public_key(path, marker):
            text = (ROOT/path).read_text().split(marker, 1)[1].split('};', 1)[0]
            return bytes(int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]{2}\b', text))
        original = public_key('WoW/Source/Client.cpp', 's_signatureModulus[256] = {')
        native = public_key('Engine/Source/BlizzardCore/Portable/Packages.cpp', 'const Bytes modulus = {')
        self.assertEqual(len(native), 256)
        self.assertEqual(native, original)
        fixture = json.loads((ROOT/'Tools/PortableAssets/signature_fixture.json').read_text())
        modulus = bytes.fromhex(fixture['modulus'])
        self.assertNotEqual(modulus, original)
        data = bytes.fromhex(fixture['signed_file'])
        self.assertEqual(data[-260:-256], b'NGIS')
        decoded = pow(int.from_bytes(data[-256:], 'little'), 65537,
                      int.from_bytes(modulus, 'little')).to_bytes(256, 'little')
        self.assertEqual(decoded, hashlib.sha1(data[:-260]).digest() + b'\xbb'*235 + b'\x0b')
        record = data[:-260].split(b'\r\n')[3].split(b';')
        digest = hashlib.md5(b'view-v').digest()
        words = ''.join(digest[i:i+4][::-1].hex() for i in range(0, 16, 4))
        self.assertEqual(record[2].decode().lower(), words)

    def test_clean_reference_inventory_is_sanitized_and_complete(self):
        inventory = json.loads((ROOT/'docs/m2/clean-data-inventory.json').read_text())
        self.assertEqual(inventory['exe_version'], [3, 3, 5, 12340])
        archives = [item for item in inventory['files'] if item['path'].endswith('.MPQ')]
        self.assertEqual(len(archives), 18)
        self.assertEqual(sum(item['bytes'] for item in archives), 17264775738)
        self.assertEqual(inventory['case_collisions'], [])
        for item in inventory['files']:
            self.assertRegex(item['sha256'], r'^[a-f0-9]{64}$')
            self.assertFalse(Path(item['path']).is_absolute())
            self.assertNotIn('..', Path(item['path']).parts)


if __name__ == '__main__':
    unittest.main()
