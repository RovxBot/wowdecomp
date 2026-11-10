"""Pinned-download transport tests; no compilation or external network."""
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import urllib.parse

from prepare import download


class PinnedDownloadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'pinned.bin'
        self.payload = b'authored pinned download fixture'
        self.sha = hashlib.sha256(self.payload).hexdigest()
        self.url = 'https://example.invalid/pinned'

    def gateway(self):
        return urllib.error.HTTPError(self.url, 504, 'Gateway timeout', {}, None)

    @patch('prepare.time.sleep')
    @patch('prepare.urllib.request.urlopen')
    def test_transient_gateway_failures_keep_checksum_requirement(self, open_url, sleep):
        open_url.side_effect = [self.gateway(), self.gateway(), io.BytesIO(self.payload)]
        download(self.url, self.path, self.sha)
        self.assertEqual(self.path.read_bytes(), self.payload)
        self.assertEqual(open_url.call_count, 3)
        for attempt, call in enumerate(open_url.call_args_list):
            parts = urllib.parse.urlsplit(call.args[0].full_url)
            self.assertEqual(urllib.parse.urlunsplit(parts._replace(query='')), self.url)
            if attempt:
                fields = urllib.parse.parse_qs(parts.query)
                self.assertEqual(fields['wowdecomp_sha256'], [self.sha])
                self.assertEqual(fields['wowdecomp_retry'], [str(attempt)])
            else:
                self.assertEqual(call.args[0].full_url, self.url)
            self.assertEqual(call.args[0].get_header('Cache-control'), 'no-cache')
        self.assertEqual([call.args[0] for call in sleep.call_args_list], [1, 2])

    @patch('prepare.time.sleep')
    @patch('prepare.urllib.request.urlopen')
    def test_transport_retry_is_bounded(self, open_url, sleep):
        open_url.side_effect = self.gateway()
        with self.assertRaises(urllib.error.HTTPError):
            download(self.url, self.path, self.sha)
        self.assertEqual(open_url.call_count, 4)
        self.assertEqual(sleep.call_count, 3)
        self.assertFalse(self.path.exists())

    @patch('prepare.time.sleep')
    @patch('prepare.urllib.request.urlopen')
    def test_permanent_failure_is_not_retried(self, open_url, sleep):
        open_url.side_effect = urllib.error.HTTPError(self.url, 404, 'Not found', {}, None)
        with self.assertRaises(urllib.error.HTTPError):
            download(self.url, self.path, self.sha)
        open_url.assert_called_once()
        sleep.assert_not_called()

    @patch('prepare.time.sleep')
    @patch('prepare.urllib.request.urlopen')
    def test_invalid_bytes_are_rejected_without_retry(self, open_url, sleep):
        open_url.return_value = io.BytesIO(b'invalid data')
        with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
            download(self.url, self.path, self.sha)
        open_url.assert_called_once()
        sleep.assert_not_called()

    @patch('prepare.time.sleep')
    @patch('prepare.urllib.request.urlopen')
    def test_retry_cannot_admit_changed_asset_bytes(self, open_url, sleep):
        open_url.side_effect = [self.gateway(), io.BytesIO(b'changed asset')]
        with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
            download(self.url, self.path, self.sha)
        self.assertEqual(open_url.call_count, 2)

    @patch('prepare.urllib.request.urlopen')
    def test_verified_cache_requires_no_network(self, open_url):
        self.path.write_bytes(self.payload)
        download(self.url, self.path, self.sha)
        open_url.assert_not_called()
