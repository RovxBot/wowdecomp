import ctypes
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from casc import FileInfo, Storage, StorageArgs, sha256
import bundle


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


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        environment = mock.patch.dict('os.environ', {'GITHUB_ACTIONS': 'false'})
        environment.start()
        self.addCleanup(environment.stop)
        self.root = Path(self.temp.name) / 'bundle'
        self.root.mkdir()
        names = ['reference/' + bundle.FONT_XML, 'assets/Fonts/FRIZQT__.TTF',
                 'assets/Fonts/MORPHEUS.ttf', 'assets/Fonts/ARIALN.TTF']
        self.files = {}
        for name in names:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'authored integrity fixture')
            self.files[name] = {'sha256': sha256(path), 'bytes': path.stat().st_size}
        self.manifest = {'schema': 1, 'scope': bundle.SCOPE, 'target_release': '0.2.0',
                         'installation': {'product': 'wowf', 'version': bundle.VERSION, 'locale': 'enUS'},
                         'files': self.files}
        bundle.write_json(self.root / 'manifest.json', self.manifest)
        self.digest = sha256(self.root / 'manifest.json')

    def test_private_assembly_copies_verified_inputs(self):
        client = Path(self.temp.name) / 'client-fixture.tar.gz'
        client.write_bytes(b'authored archive fixture, not a client executable')
        output = Path(self.temp.name) / 'release'
        args = argparse.Namespace(bundle=self.root, manifest_sha256=self.digest,
                                  client=client, client_sha256=sha256(client), output=output)
        bundle.prepare_release(args)
        bundle.verify(output / 'forever-bootstrap', self.digest)
        self.assertEqual((output / 'client' / client.name).read_bytes(), client.read_bytes())
        with self.assertRaisesRegex(ValueError, 'new and separate'):
            bundle.prepare_release(args)

    def test_changed_asset_fails_release_gate(self):
        (self.root / 'assets/Fonts/ARIALN.TTF').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
            bundle.verify(self.root, self.digest)

    def test_changed_manifest_rejected(self):
        (self.root / 'manifest.json').write_text('{}')
        with self.assertRaisesRegex(ValueError, 'manifest checksum mismatch'):
            bundle.verify(self.root, self.digest)

    def test_missing_required_asset_rejected(self):
        del self.files['assets/Fonts/ARIALN.TTF']
        bundle.write_json(self.root / 'manifest.json', self.manifest)
        with self.assertRaisesRegex(ValueError, 'Required'):
            bundle.verify(self.root, sha256(self.root / 'manifest.json'))

    def test_unlisted_file_rejected(self):
        (self.root / 'extra-file').write_text('unexpected')
        with self.assertRaisesRegex(ValueError, 'Unlisted'):
            bundle.verify(self.root, self.digest)

    def test_symlink_and_traversal_rejected(self):
        name = 'assets/Fonts/ARIALN.TTF'
        (self.root / name).unlink()
        (self.root / name).symlink_to(self.root / 'assets/Fonts/MORPHEUS.ttf')
        with self.assertRaisesRegex(ValueError, 'symlinked'):
            bundle.verify(self.root, self.digest)
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            bundle.safe_file(self.root, '../manifest.json')

    def test_non_roman_fonts_not_required_for_enUS(self):
        xml = Path(self.temp.name) / 'fonts.xml'
        xml.write_text('<Ui><FontFamily><Member alphabet="roman"><Font font="Fonts\\Roman.ttf"/></Member>'
                       '<Member alphabet="korean"><Font font="Fonts\\Korean.ttf"/></Member></FontFamily></Ui>')
        self.assertEqual(bundle.roman_fonts(xml), ['Fonts/Roman.ttf'])

    def test_output_in_repository_rejected(self):
        with self.assertRaisesRegex(ValueError, 'outside Git'):
            bundle.external(bundle.REPO / 'assets')

    def test_release_output_in_game_install_rejected(self):
        install = Path(self.temp.name) / 'installation'
        (install / 'Data').mkdir(parents=True)
        (install / 'Data/base.MPQ').write_bytes(b'authored archive marker')
        with self.assertRaisesRegex(ValueError, 'outside game installations'):
            bundle.external(install / 'private-release')

    def test_ci_rejects_private_staging_and_assembly(self):
        with mock.patch.dict('os.environ', {'GITHUB_ACTIONS': 'true'}):
            with self.assertRaisesRegex(ValueError, 'outside ordinary CI'):
                bundle.stage(None)
            with self.assertRaisesRegex(ValueError, 'outside ordinary CI'):
                bundle.prepare_release(None)


if __name__ == '__main__':
    unittest.main()
