"""Generate redistributable MPQs/codec vectors, never using game data or keys."""
import bz2
import hashlib
import json
import lzma
from pathlib import Path
import struct
import sys
import zlib

MASK = 0xffffffff
TABLE = [0] * 1280
seed = 0x100001
for i in range(256):
    for j in range(5):
        seed = (seed * 125 + 3) % 0x2aaaab
        high = (seed & 0xffff) << 16
        seed = (seed * 125 + 3) % 0x2aaaab
        TABLE[i + j * 256] = high | (seed & 0xffff)


def hash_name(name, kind):
    a, b = 0x7fed7fed, 0xeeeeeeee
    for c in name.upper().replace('/', '\\').encode():
        a = TABLE[kind * 256 + c] ^ ((a + b) & MASK)
        b = (c + a + b + (b << 5) + 3) & MASK
    return a


def encrypt(data, key):
    data = bytearray(data)
    seed = 0xeeeeeeee
    for pos in range(0, len(data) - 3, 4):
        seed = (seed + TABLE[0x400 + (key & 255)]) & MASK
        word = struct.unpack_from('<I', data, pos)[0]
        struct.pack_into('<I', data, pos, word ^ ((key + seed) & MASK))
        key = (((~key << 21) + 0x11111111) | (key >> 11)) & MASK
        seed = (word + seed + (seed << 5) + 3) & MASK
    return bytes(data)


def mpq(entries, version=1):
    hashes = [b'\xff' * 16 for _ in range(32)]
    blocks = []
    data = bytearray(44 if version else 32)
    for name, content, mode in entries:
        offset = len(data)
        key = hash_name(name.replace('/', '\\').split('\\')[-1], 3)
        flags = 0x80000000
        if mode == 'delete':
            flags |= 0x02000000
            packed = b''
        elif mode == 'single':
            flags |= 0x01000200
            packed = b'\x02' + zlib.compress(content)
            if len(packed) >= len(content):
                packed = content
        elif mode == 'encrypted':
            flags |= 0x00010000
            packed = b''.join(encrypt(content[i:i+4096], (key+i//4096) & MASK)
                              for i in range(0, len(content), 4096))
        elif mode in ('sectors', 'crc', 'badcrc'):
            flags |= 0x00030200
            key = ((key + offset) ^ len(content)) & MASK
            chunks = [content[i:i+4096] for i in range(0, len(content), 4096)]
            encoded, checks = [], []
            for chunk in chunks:
                encoded_chunk = b'\x02' + zlib.compress(chunk)
                if len(encoded_chunk) >= len(chunk):
                    encoded_chunk = chunk
                checks.append(zlib.adler32(encoded_chunk, 0) or MASK)
                encoded.append(encrypt(encoded_chunk, (key + len(encoded)) & MASK))
            offsets = [(len(chunks) + 1 + (mode != 'sectors')) * 4]
            for chunk in encoded:
                offsets.append(offsets[-1] + len(chunk))
            crc = b''
            if mode != 'sectors':
                flags |= 0x04000000
                if mode == 'badcrc':
                    checks[0] ^= 0x1234
                crc = struct.pack('<' + 'I'*len(checks), *checks)
                offsets.append(offsets[-1] + len(crc))
            packed = encrypt(struct.pack('<'+'I'*len(offsets), *offsets), (key-1) & MASK) + b''.join(encoded) + crc
        else:
            packed = content
        block = struct.pack('<IIII', offset, len(packed), len(content), flags)
        slot = hash_name(name, 0) % len(hashes)
        while hashes[slot] != b'\xff'*16:
            slot = (slot + 1) % len(hashes)
        hashes[slot] = struct.pack('<IIHHI', hash_name(name, 1), hash_name(name, 2), 0, 0, len(blocks))
        blocks.append(block)
        data += packed
    hash_offset = len(data)
    data += encrypt(b''.join(hashes), hash_name('(hash table)', 3))
    block_offset = len(data)
    data += encrypt(b''.join(blocks), hash_name('(block table)', 3))
    header = struct.pack('<4sIIHHIIII', b'MPQ\x1a', 44 if version else 32, len(data), version, 3,
                         hash_offset, block_offset, len(hashes), len(blocks))
    if version:
        header += bytes(12)
    data[:len(header)] = header
    return data


def main(directory):
    directory.mkdir(parents=True, exist_ok=True)
    key = json.loads(Path(__file__).with_name('signature_fixture.json').read_text())
    signature = bytes.fromhex(key['signed_file'])
    (directory/'modulus.bin').write_bytes(bytes.fromhex(key['modulus']))
    content = bytes(range(256)) * 51 + b'end'
    base = [('shared.txt', b'base-v', 'raw'), ('gone.txt', b'old-v', 'raw'),
            ('locale.txt', b'locale-v', 'raw'), ('expansion.txt', b'expansion-v', 'raw'),
            ('empty.txt', b'', 'raw'), ('encrypted.bin', content, 'encrypted'),
            ('sector.bin', content, 'sectors'), ('crc.bin', content, 'crc'),
            ('badcrc.bin', content, 'badcrc'), ('single.bin', content, 'single'),
            ('raw.bin', content, 'raw'), ('corrupt.txt', b'wrong-v', 'raw')]
    patch = [('signaturefile', signature, 'sectors'), ('shared.txt', b'patch-v', 'raw'),
             ('expansion/shared.txt', b'view-v', 'raw'), ('gone.txt', b'', 'delete')]
    (directory/'base.mpq').write_bytes(mpq(base))
    (directory/'patch.mpq').write_bytes(mpq(patch))
    (directory/'v0.mpq').write_bytes(mpq([('plain.txt', b'plain-v', 'raw')], version=0))
    (directory/'expected.bin').write_bytes(content)
    (directory/'signature.bin').write_bytes(signature)
    (directory/'codec-zlib.bin').write_bytes(b'\x02' + zlib.compress(content))
    (directory/'codec-bzip2.bin').write_bytes(b'\x10' + bz2.compress(content))
    # Original MPQ LZMA: type 0x12, filter byte 0, five properties, uint64 size.
    alone = lzma.compress(content, format=lzma.FORMAT_ALONE)
    (directory/'codec-lzma.bin').write_bytes(b'\x12\x00' + alone[:5] + struct.pack('<Q',len(content)) + alone[13:])
    # MPQ sparse encodes a big-endian output length, zero runs and literals.
    sparse = struct.pack('>I', 100) + bytes([97])
    (directory/'codec-sparse.bin').write_bytes(b'\x20' + sparse)
    (directory/'codec-zlib-sparse.bin').write_bytes(b'\x22' + zlib.compress(sparse))
    (directory/'codec-bzip2-sparse.bin').write_bytes(b'\x30' + bz2.compress(sparse))
    bundled = Path(__file__).resolve().parents[2] / 'ThirdParty/zlib/contrib/blast'
    (directory/'codec-pkware.bin').write_bytes((bundled/'test.pk').read_bytes())
    (directory/'pkware-expected.bin').write_bytes((bundled/'test.txt').read_bytes())
    # Mark Adler-0 conventions explicitly; this differs from zlib's default seed.
    (directory/'fixture-identity.json').write_text(json.dumps({
        'scope':'synthetic redistributable fixtures; no Blizzard data/signing key',
        'content_sha256': hashlib.sha256(content).hexdigest()}, indent=2)+'\n')


if __name__ == '__main__':
    main(Path(sys.argv[1]))
