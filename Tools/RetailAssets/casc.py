"""Bounded offline CascLib bridge. No online or encryption-overcome flags."""
import ctypes as C
import hashlib
import json
from pathlib import Path


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class StorageArgs(C.Structure):
    _fields_ = [('Size', C.c_size_t)] + [
        (name, C.c_char_p) for name in ('path', 'product', 'region')
    ] + [(name, C.c_void_p) for name in ('progress', 'progress_arg', 'select', 'select_arg')] + [
        ('locale', C.c_uint32), ('flags', C.c_uint32),
        ('build', C.c_char_p), ('cdn', C.c_char_p)]


class FileInfo(C.Structure):
    _fields_ = [('ckey', C.c_ubyte * 16), ('ekey', C.c_ubyte * 16),
                ('data_file', C.c_char * 16)] + [
        (name, C.c_uint64) for name in ('storage_offset', 'segment_offset', 'tags',
                                      'name_hash', 'size', 'encoded_size')
    ] + [(name, C.c_uint32) for name in ('segment', 'spans', 'file_id', 'locale', 'content')]


class Storage:
    def __init__(self, library, digest, install, build):
        library = Path(library).resolve()
        lock = json.loads(Path(__file__).with_name('reader.json').read_text())
        manifest = json.loads(library.with_name('manifest.json').read_text())
        if manifest['source'] != lock or sha256(library) != digest:
            raise ValueError('Unverified CASC reader/source identity')
        if manifest['files'][library.name] != digest:
            raise ValueError('Reader manifest checksum mismatch')
        for name, expected in manifest['files'].items():
            if Path(name).name != name or sha256(library.with_name(name)) != expected:
                raise ValueError('Reader artifact checksum mismatch')
        self.library = C.CDLL(str(library))
        signatures = {
            'CascOpenStorageEx': ([C.c_char_p, C.POINTER(StorageArgs), C.c_bool, C.POINTER(C.c_void_p)], C.c_bool),
            'CascCloseStorage': ([C.c_void_p], C.c_bool),
            'CascOpenFile': ([C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.POINTER(C.c_void_p)], C.c_bool),
            'CascCloseFile': ([C.c_void_p], C.c_bool),
            'CascGetFileInfo': ([C.c_void_p, C.c_int, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)], C.c_bool),
            'CascGetFileSize64': ([C.c_void_p, C.POINTER(C.c_uint64)], C.c_bool),
            'CascReadFile': ([C.c_void_p, C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32)], C.c_bool),
            'GetCascError': ([], C.c_uint32),
        }
        for name, (args, result) in signatures.items():
            fn = getattr(self.library, name)
            fn.argtypes, fn.restype = args, result
        self.handle = C.c_void_p()
        args = StorageArgs()
        args.Size = C.sizeof(args)
        args.path = str(Path(install).resolve()).encode()
        args.product = b'wowf'
        args.build = build.encode('ascii')
        args.locale = 2  # CASC_LOCALE_ENUS
        # dwFlags=0 excludes CASC_FEATURE_ALLOW_DOWNLOAD; bOnlineStorage=false.
        if not self.library.CascOpenStorageEx(None, C.byref(args), False, C.byref(self.handle)):
            self.error('Open offline storage')

    def error(self, operation):
        raise OSError(self.library.GetCascError(), operation)

    def close(self):
        if self.handle:
            self.library.CascCloseStorage(self.handle)
            self.handle = C.c_void_p()

    def read(self, name):
        handle = C.c_void_p()
        encoded = name.replace('/', '\\').encode('utf-8')
        # CASC_OPEN_BY_NAME | CASC_STRICT_DATA_CHECK. Never OVERCOME_ENCRYPTED.
        if not self.library.CascOpenFile(self.handle, C.c_char_p(encoded), 2, 0x10, C.byref(handle)):
            self.error('Open ' + name)
        try:
            size = C.c_uint64()
            if not self.library.CascGetFileSize64(handle, C.byref(size)):
                self.error('Size ' + name)
            if not 0 < size.value <= 64 * 1024 * 1024:
                raise ValueError('Asset size outside UI/font bound: ' + name)
            data = C.create_string_buffer(size.value)
            count = C.c_uint32()
            if not self.library.CascReadFile(handle, data, size.value, C.byref(count)):
                self.error('Read ' + name)
            if count.value != size.value:
                raise ValueError('Truncated CASC asset: ' + name)
            info = FileInfo()
            needed = C.c_size_t()
            if not self.library.CascGetFileInfo(handle, 2, C.byref(info), C.sizeof(info), C.byref(needed)):
                self.error('Identity ' + name)
            return data.raw, {'ckey': bytes(info.ckey).hex(), 'ekey': bytes(info.ekey).hex(),
                              'file_data_id': info.file_id, 'locale_flags': info.locale}
        finally:
            self.library.CascCloseFile(handle)
