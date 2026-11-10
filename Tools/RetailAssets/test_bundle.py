import ctypes
import json
from pathlib import Path
import tempfile
import unittest

from casc import FileInfo, Storage, StorageArgs, sha256


class ReaderTests(unittest.TestCase):
    def test_pinned_linux_x64_abi(self):
        self.assertEqual(ctypes.sizeof(StorageArgs), 88)
        self.assertEqual(FileInfo.file_id.offset, 104)
        self.assertEqual(ctypes.sizeof(FileInfo), 120)

    def test_reject_reader_tampering_before_loading(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / 'libcasc.so.1.0.0'
            library.write_bytes(b'not an executable library')
            lock = json.loads(Path(__file__).with_name('reader.json').read_text())
            (root / 'manifest.json').write_text(json.dumps({'source': lock, 'files': {library.name: sha256(library)}}))
            with self.assertRaisesRegex(ValueError, 'Unverified'):
                Storage(library, '0' * 64, root, '0' * 32)


if __name__ == '__main__':
    unittest.main()
