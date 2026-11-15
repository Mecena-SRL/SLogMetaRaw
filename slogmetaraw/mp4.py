# SPDX-License-Identifier: GPL-3.0-or-later
"""Minimal ISO-BMFF (MP4/MOV) reader.

Reads only box headers and the few boxes we need (moov, top-level meta,
sample tables), so even multi-GB clips cost a few KB of I/O.
"""
import os
import struct
import threading

MAX_BOXES = 4096               # per level: a non-ISO file must not become a long walk
PRELOAD_MAX = 64 * 1024 * 1024
META_PRELOAD = 64 * 1024


class CountingFile:
    """Unbuffered positional reads, counted for the performance tests; preload() keeps one window in memory.

    A buffered file would read st_blksize (1 MiB on exFAT) for every 2 KB sample.
    """

    def __init__(self, path):
        # O_BINARY: Windows would otherwise translate line endings in the bytes it hands back
        self._fd = os.open(path, os.O_RDONLY | getattr(os, 'O_BINARY', 0))
        self._lock = threading.Lock()   # Windows has no pread: seek + read must not interleave
        try:
            self.size = os.fstat(self._fd).st_size
        except OSError:
            os.close(self._fd)
            raise
        self.bytes_read = 0
        self.reads = 0
        self._base = 0
        self._window = b''

    def _pread(self, pos, n):
        n = max(0, min(n, self.size - pos))
        if n == 0 or pos < 0:
            return b''
        data = self._read_range(pos, n)
        self.reads += 1
        self.bytes_read += len(data)
        return data

    def _read_range(self, pos, n):
        if hasattr(os, 'pread'):
            return os.pread(self._fd, n, pos)
        with self._lock:
            os.lseek(self._fd, pos, os.SEEK_SET)
            chunks = []
            while n > 0:
                chunk = os.read(self._fd, n)
                if not chunk:
                    break
                chunks.append(chunk)
                n -= len(chunk)
            return b''.join(chunks)

    def preload(self, pos, n):
        rel = pos - self._base
        if not (rel >= 0 and rel + min(n, self.size - pos) <= len(self._window)):
            self._base, self._window = pos, self._pread(pos, n)

    def read_at(self, pos, n):
        rel = pos - self._base
        if rel >= 0 and rel + n <= len(self._window):
            return self._window[rel:rel + n]
        return self._pread(pos, n)

    def close(self):
        os.close(self._fd)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def iter_boxes(f, start, end):
    """Yield (type, box_start, header_len, box_size) for boxes in [start, end)."""
    pos = start
    for _ in range(MAX_BOXES):
        if pos + 8 > end:
            return
        hdr = f.read_at(pos, 16)
        if len(hdr) < 8:
            return
        size, typ = struct.unpack('>I4s', hdr[:8])
        hlen = 8
        if size == 1:
            if len(hdr) < 16:
                return
            size = struct.unpack('>Q', hdr[8:16])[0]
            hlen = 16
        elif size == 0:
            size = end - pos
        size = min(size, end - pos)   # a corrupt size must not read past the parent
        if size < hlen:
            return
        yield typ, pos, hlen, size
        pos += size


KEYS_MAX = 256 * 1024             # one metadata value (Panasonic's clip XML is ~2 KB)
DATA_TYPES = {1: lambda v: v.decode('utf-8', 'replace'), 21: lambda v: int.from_bytes(v, 'big', signed=True),
              22: lambda v: int.from_bytes(v, 'big'), 23: lambda v: struct.unpack('>f', v)[0],
              24: lambda v: struct.unpack('>d', v)[0]}


def _data_value(d):
    """Value of an ilst 'data' box: type indicator, locale, payload."""
    if len(d) < 8:
        return None
    v = d[8:]
    try:
        return DATA_TYPES[struct.unpack('>I', d[:4])[0] & 0xFFFFFF](v)
    except (KeyError, struct.error):
        return v


class Track:
    def __init__(self):
        self.handler = None
        self.codec = None          # sample entry fourcc
        self.sample_entry = b''    # full first sample entry bytes
        self.timescale = None
        self.duration = None
        self.tables = {}           # b'stsz' -> payload bytes, etc.

    # -- sample table helpers -------------------------------------------
    def sample_count(self):
        stsz = self.tables.get(b'stsz')
        if not stsz or len(stsz) < 12:
            return 0
        fixed, count = struct.unpack('>II', stsz[4:12])
        return count if fixed else min(count, (len(stsz) - 12) // 4)

    def sample_size(self, idx):
        stsz = self.tables[b'stsz']
        fixed = struct.unpack_from('>I', stsz, 4)[0]
        return fixed or struct.unpack_from('>I', stsz, 12 + 4 * idx)[0]

    def _chunk_table(self):
        """(payload, format, entry size, entries) of stco or co64, read in place: a 3 h clip has ~650k chunks."""
        d, fmt, width = (self.tables[b'stco'], '>I', 4) if b'stco' in self.tables else (self.tables[b'co64'], '>Q', 8)
        n = min(struct.unpack_from('>I', d, 4)[0], (len(d) - 8) // width) if len(d) >= 8 else 0
        return d, fmt, width, n

    def chunk_count(self):
        return self._chunk_table()[3]

    def sample_offsets(self, wanted):
        """Return {sample_index: file_offset} for the requested sample indices."""
        count = self.sample_count()
        wanted = sorted(set(i for i in wanted if 0 <= i < count))
        if not wanted or b'stsc' not in self.tables or not (b'stco' in self.tables or b'co64' in self.tables):
            return {}
        stsc = self.tables[b'stsc']
        if len(stsc) < 8:
            return {}
        n = min(struct.unpack('>I', stsc[4:8])[0], (len(stsc) - 8) // 12)
        runs = [struct.unpack('>III', stsc[8 + 12 * i:20 + 12 * i]) for i in range(n)]
        cdata, cfmt, cwidth, nchunks = self._chunk_table()
        out = {}
        stsz = self.tables[b'stsz']
        fixed = struct.unpack_from('>I', stsz, 4)[0]
        sample = 0
        w = 0
        for ri, (first_chunk, per_chunk, _desc) in enumerate(runs):
            last_chunk = min(runs[ri + 1][0] - 1 if ri + 1 < len(runs) else nchunks, nchunks)
            if per_chunk == 0 or first_chunk < 1:
                continue   # a damaged run: skipping it must not walk billions of empty chunks
            for c in range(first_chunk, last_chunk + 1):
                if w >= len(wanted):
                    return out
                chunk_end = sample + per_chunk
                if wanted[w] >= chunk_end:
                    sample = chunk_end
                    continue
                off = struct.unpack_from(cfmt, cdata, 8 + cwidth * (c - 1))[0]
                for s in range(sample, chunk_end):
                    if s == wanted[w]:
                        out[s] = off
                        w += 1
                        if w == len(wanted):
                            return out
                    off += fixed or struct.unpack_from('>I', stsz, 12 + 4 * s)[0]
                sample = chunk_end
        return out


def _parse_trak(f, trak):
    tr = Track()
    p, hl, s = trak

    def walk(a, b):
        for t, bp, bhl, bs in iter_boxes(f, a, b):
            if t in (b'mdia', b'minf'):
                walk(bp + bhl, bp + bs)
            elif t == b'mdhd':
                d = f.read_at(bp + bhl, bs - bhl)
                if len(d) >= 32 and d[0] == 1:
                    tr.timescale, tr.duration = struct.unpack('>IQ', d[20:32])
                elif len(d) >= 20:
                    tr.timescale, tr.duration = struct.unpack('>II', d[12:20])
            elif t == b'hdlr' and tr.handler is None:
                # QuickTime repeats hdlr inside minf (data handler 'alis'): the media one comes first
                tr.handler = f.read_at(bp + bhl + 8, 4)
            elif t == b'stbl':
                for t2, p2, hl2, s2 in iter_boxes(f, bp + bhl, bp + bs):
                    if t2 in (b'stsd', b'stsz', b'stco', b'co64', b'stsc', b'stts'):
                        tr.tables[t2] = f.read_at(p2 + hl2, s2 - hl2)
                stsd = tr.tables.get(b'stsd')
                if stsd and len(stsd) >= 16:
                    esize = struct.unpack('>I', stsd[8:12])[0]
                    tr.codec = stsd[12:16]
                    tr.sample_entry = stsd[8:8 + esize]

    walk(p + hl, p + s)
    return tr


class MP4:
    def __init__(self, f):
        self.f = f
        self.tracks = []
        self.meta_xml = None          # NonRealTimeMeta XML (bytes)
        self.items = {}               # item name -> (file offset, length), e.g. 'Look Control data'
        self.keys = {}                # QuickTime moov/meta 'mdta' keys -> value (str, int, float or bytes)
        self.udta = {}                # moov/udta atoms -> payload bytes (text atoms decoded to str)
        self._parse()

    def _parse(self):
        f = self.f
        f.preload(0, META_PRELOAD)     # ftyp, uuid and free headers in one read
        for t, p, hl, s in iter_boxes(f, 0, f.size):
            if t == b'moov':
                if s <= PRELOAD_MAX:
                    f.preload(p, s + 16)   # + the next top-level header
                for t2, p2, hl2, s2 in iter_boxes(f, p + hl, p + s):
                    if t2 == b'trak':
                        self.tracks.append(_parse_trak(f, (p2, hl2, s2)))
                    elif t2 == b'meta':
                        self._parse_keys(p2 + hl2, p2 + s2)
                    elif t2 == b'udta':
                        self._parse_udta(p2 + hl2, p2 + s2)
            elif t == b'meta':
                f.preload(p, min(s, META_PRELOAD))
                self._parse_meta(p + hl + 4, p + s)

    def _parse_keys(self, a, b):
        """moov/meta as QuickTime writes it (handler 'mdta'): 'keys' names, 'ilst' values by 1-based index."""
        f = self.f
        if f.read_at(a + 4, 4) != b'hdlr':
            a += 4                     # the ISO form is a full box: version and flags first
        names, values = {}, {}
        for t, p, hl, s in iter_boxes(f, a, b):
            d = f.read_at(p + hl, min(s - hl, KEYS_MAX))
            if t == b'keys' and len(d) >= 8:
                q = 8
                for i in range(1, struct.unpack('>I', d[4:8])[0] + 1):
                    size = struct.unpack('>I', d[q:q + 4])[0] if q + 8 <= len(d) else 0
                    if size < 8:
                        break
                    names[i] = d[q + 8:q + size].decode('utf-8', 'replace')
                    q += size
            elif t == b'ilst':
                for t2, p2, hl2, s2 in iter_boxes(f, p + hl, p + s):
                    for t3, p3, hl3, s3 in iter_boxes(f, p2 + hl2, p2 + s2):
                        if t3 == b'data':
                            values[struct.unpack('>I', t2)[0]] = _data_value(f.read_at(p3 + hl3, s3 - hl3))
                            break
        self.keys.update({names[i]: v for i, v in values.items() if i in names})

    def _parse_udta(self, a, b):
        """moov/udta atoms; '\xa9xxx' text atoms (maker, model, software) as strings."""
        for t, p, hl, s in iter_boxes(self.f, a, b):
            d = self.f.read_at(p + hl, min(s - hl, KEYS_MAX))
            name = t.decode('latin1')
            if t[:1] == b'\xa9' and len(d) >= 4:
                n = struct.unpack('>H', d[:2])[0]
                d = d[4:4 + n].decode('utf-8', 'replace').rstrip('\0')
            self.udta[name] = d

    def _parse_meta(self, a, b):
        """Top-level 'meta' (handler 'nrtm'): xml box + items in idat."""
        f = self.f
        names = {}
        locs = {}
        idat_start = None
        for t, p, hl, s in iter_boxes(f, a, b):
            if t == b'xml ':
                self.meta_xml = f.read_at(p + hl + 4, s - hl - 4)
            elif t == b'iinf':
                d = f.read_at(p + hl, s - hl)
                if len(d) < 6:
                    continue
                ver = d[0]
                q = 6 if ver == 0 else 8
                while q + 14 <= len(d):
                    size = struct.unpack('>I', d[q:q + 4])[0]
                    if size < 8:
                        break
                    e = d[q + 8:q + size]
                    if len(e) < 8:
                        break
                    ev = e[0]
                    if ev >= 3:   # 32-bit item_ID
                        item_id = struct.unpack('>I', e[4:8])[0]
                        name = e[14:].split(b'\0')[0]
                    elif ev == 2:
                        item_id = struct.unpack('>H', e[4:6])[0]
                        name = e[12:].split(b'\0')[0]
                    else:
                        item_id = struct.unpack('>H', e[4:6])[0]
                        name = e[8:].split(b'\0')[0]
                    names[item_id] = name.decode('utf-8', 'replace')
                    q += size
            elif t == b'iloc':
                d = f.read_at(p + hl, s - hl)
                if len(d) < 8:
                    continue
                ver = d[0]
                sz = d[4]
                off_size, len_size = sz >> 4, sz & 15
                base_size = d[5] >> 4
                idx_size = d[5] & 15 if ver in (1, 2) else 0
                q = 6

                def rd(n):
                    nonlocal q
                    v = int.from_bytes(d[q:q + n], 'big') if n else 0
                    q += n
                    return v

                id_size = 4 if ver == 2 else 2   # ISO 14496-12: 32-bit count and item_ID in version 2
                count = rd(id_size)
                for _ in range(count):
                    if q >= len(d):
                        break
                    item_id = rd(id_size)
                    method = 0
                    if ver in (1, 2):
                        method = rd(2) & 15
                    rd(2)  # data_reference_index
                    base = rd(base_size)
                    ext = min(rd(2), (len(d) - q) // max(1, idx_size + off_size + len_size))
                    for _e in range(ext):
                        if idx_size:
                            rd(idx_size)
                        eo = rd(off_size)
                        el = rd(len_size)
                        locs.setdefault(item_id, (method, base + eo, el))
            elif t == b'idat':
                idat_start = p + hl
        for item_id, (method, off, length) in locs.items():
            name = names.get(item_id, 'item%d' % item_id)
            if method == 1 and idat_start is not None:
                self.items[name] = (idat_start + off, length)
            elif method == 0:
                self.items[name] = (off, length)

    def item(self, name):
        """Bytes of a meta item, read only on request (the LUT alone is ~434 KB)."""
        loc = self.items.get(name)
        return self.f.read_at(*loc) if loc else None

    def track(self, handler=None, codec=None):
        for t in self.tracks:
            if (handler is None or t.handler == handler) and (codec is None or t.codec == codec):
                return t
        return None
