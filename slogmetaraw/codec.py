# SPDX-License-Identifier: GPL-3.0-or-later
"""H.264 / HEVC sequence parameter set parsing.

Extracts profile, level, chroma format, bit depth and the VUI video signal
type (full/video range flag, colour primaries, transfer, matrix) from the
avcC / hvcC configuration record of an MP4 sample entry.
"""
import struct

PRIMARIES = {1: 'BT.709', 2: 'Unspecified', 4: 'BT.470M', 5: 'BT.470BG', 6: 'SMPTE 170M',
             7: 'SMPTE 240M', 8: 'Film', 9: 'BT.2020', 10: 'SMPTE ST 428', 11: 'DCI-P3',
             12: 'Display P3'}
TRANSFER = {1: 'BT.709', 2: 'Unspecified', 4: 'Gamma 2.2', 5: 'Gamma 2.8', 6: 'SMPTE 170M',
            7: 'SMPTE 240M', 8: 'Linear', 11: 'IEC 61966-2-4', 13: 'sRGB', 14: 'BT.2020 10-bit',
            15: 'BT.2020 12-bit', 16: 'SMPTE ST 2084 (PQ)', 18: 'ARIB STD-B67 (HLG)'}
MATRIX = {0: 'Identity', 1: 'BT.709', 2: 'Unspecified', 5: 'BT.470BG', 6: 'SMPTE 170M',
          7: 'SMPTE 240M', 9: 'BT.2020 NCL', 10: 'BT.2020 CL'}
CHROMA = {0: '4:0:0', 1: '4:2:0', 2: '4:2:2', 3: '4:4:4'}
AVC_PROFILES = {66: 'Baseline', 77: 'Main', 88: 'Extended', 100: 'High', 110: 'High10',
                122: 'High422', 244: 'High444'}
HEVC_PROFILES = {1: 'Main', 2: 'Main10', 3: 'Main Still', 4: 'Main 4:2:2 10'}


def _unescape(nal):
    """Remove emulation prevention bytes (00 00 03)."""
    out = bytearray()
    zeros = 0
    for b in nal:
        if zeros >= 2 and b == 3:
            zeros = 0
            continue
        out.append(b)
        zeros = zeros + 1 if b == 0 else 0
    return bytes(out)


class Bits:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def u(self, n):
        v = 0
        for _ in range(n):
            byte = self.d[self.p >> 3]
            v = (v << 1) | ((byte >> (7 - (self.p & 7))) & 1)
            self.p += 1
        return v

    def ue(self):
        z = 0
        while self.u(1) == 0:
            z += 1
            if z > 31:
                raise ValueError('bad exp-golomb')
        return (1 << z) - 1 + self.u(z)

    def se(self):
        k = self.ue()
        return (k + 1) // 2 if k & 1 else -(k // 2)


def _vui_signal(b, out):
    if b.u(1):  # aspect_ratio_info_present_flag
        if b.u(8) == 255:
            b.u(16)
            b.u(16)
    if b.u(1):  # overscan_info_present_flag
        b.u(1)
    if b.u(1):  # video_signal_type_present_flag
        b.u(3)  # video_format
        out['full_range'] = bool(b.u(1))
        if b.u(1):  # colour_description_present_flag
            p, t, m = b.u(8), b.u(8), b.u(8)
            out['vui_primaries'] = PRIMARIES.get(p, str(p))
            out['vui_transfer'] = TRANSFER.get(t, str(t))
            out['vui_matrix'] = MATRIX.get(m, str(m))
    else:
        out['full_range'] = None  # not signalled -> decoders assume video range


def _skip_scaling_list(b, size):
    last, nxt = 8, 8
    for _ in range(size):
        if nxt:
            nxt = (last + b.se() + 256) % 256
        last = nxt or last


def parse_avc_sps(nal):
    b = Bits(_unescape(nal[1:]))
    out = {'codec': 'H.264'}
    profile = b.u(8)
    b.u(8)
    level = b.u(8)
    out['profile'] = AVC_PROFILES.get(profile, str(profile))
    out['level'] = '%g' % (level / 10)
    b.ue()
    chroma, depth_l = 1, 8
    if profile in (100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135):
        chroma = b.ue()
        if chroma == 3:
            b.u(1)
        depth_l = b.ue() + 8
        b.ue()   # chroma bit depth
        b.u(1)
        if b.u(1):
            for i in range(8 if chroma != 3 else 12):
                if b.u(1):
                    _skip_scaling_list(b, 16 if i < 6 else 64)
    out['chroma'] = CHROMA.get(chroma, str(chroma))
    out['bit_depth'] = depth_l
    b.ue()  # log2_max_frame_num_minus4
    poc = b.ue()
    if poc == 0:
        b.ue()
    elif poc == 1:
        b.u(1)
        b.se()
        b.se()
        for _ in range(b.ue()):
            b.se()
    b.ue()
    b.u(1)
    w = (b.ue() + 1) * 16
    h_units = b.ue() + 1
    frame_mbs_only = b.u(1)
    h = h_units * 16 * (2 - frame_mbs_only)
    if not frame_mbs_only:
        b.u(1)
    b.u(1)
    if b.u(1):  # cropping
        cl, cr, ct, cb = b.ue(), b.ue(), b.ue(), b.ue()
        sx = 1 if chroma in (0, 3) else 2
        sy = (2 if chroma == 1 else 1) * (2 - frame_mbs_only)
        w -= (cl + cr) * sx
        h -= (ct + cb) * sy
    out['width'], out['height'] = w, h
    out['interlaced'] = not frame_mbs_only
    if b.u(1):
        _vui_signal(b, out)
    else:
        out['full_range'] = None
    return out


def _hevc_ptl(b, max_sub_layers_minus1, out):
    b.u(2)
    b.u(1)
    profile = b.u(5)
    b.u(32)
    b.u(4)
    b.u(43)
    b.u(1)
    level = b.u(8)
    out['profile'] = HEVC_PROFILES.get(profile, str(profile))
    out['level'] = '%g' % (level / 30)
    sub_prof, sub_lev = [], []
    for _ in range(max_sub_layers_minus1):
        sub_prof.append(b.u(1))
        sub_lev.append(b.u(1))
    if max_sub_layers_minus1 > 0:
        for _ in range(max_sub_layers_minus1, 8):
            b.u(2)
    for i in range(max_sub_layers_minus1):
        if sub_prof[i]:
            b.u(88)
        if sub_lev[i]:
            b.u(8)


def _hevc_scaling_list_data(b):
    for size_id in range(4):
        for _matrix in range(0, 6, 3 if size_id == 3 else 1):
            if not b.u(1):
                b.ue()
            else:
                n = min(64, 1 << (4 + (size_id << 1)))
                if size_id > 1:
                    b.se()
                for _ in range(n):
                    b.se()


def parse_hevc_sps(nal):
    b = Bits(_unescape(nal[2:]))
    out = {'codec': 'H.265'}
    b.u(4)
    msl = b.u(3)
    b.u(1)
    _hevc_ptl(b, msl, out)
    b.ue()
    chroma = b.ue()
    if chroma == 3:
        b.u(1)
    w, h = b.ue(), b.ue()
    if b.u(1):
        sx = 1 if chroma in (0, 3) else 2
        sy = 2 if chroma == 1 else 1
        cl, cr, ct, cb = b.ue(), b.ue(), b.ue(), b.ue()
        w -= (cl + cr) * sx
        h -= (ct + cb) * sy
    out['width'], out['height'] = w, h
    out['chroma'] = CHROMA.get(chroma, str(chroma))
    out['bit_depth'] = b.ue() + 8
    b.ue()
    log2_poc = b.ue() + 4
    present = b.u(1)
    for _ in range(0 if present else msl, msl + 1):
        b.ue()
        b.ue()
        b.ue()
    for _ in range(6):
        b.ue()
    if b.u(1) and b.u(1):
        _hevc_scaling_list_data(b)
    b.u(1)
    b.u(1)
    if b.u(1):  # pcm
        b.u(4)
        b.u(4)
        b.ue()
        b.ue()
        b.u(1)
    num_sets = b.ue()
    num_delta = []
    for idx in range(num_sets):
        inter = b.u(1) if idx else 0
        if inter:
            b.u(1)
            b.ue()
            n = 0
            for _ in range(num_delta[idx - 1] + 1):
                used = b.u(1)
                use_delta = 1 if used else b.u(1)
                n += 1 if (used or use_delta) else 0
            num_delta.append(n)
        else:
            neg, pos = b.ue(), b.ue()
            for _ in range(neg + pos):
                b.ue()
                b.u(1)
            num_delta.append(neg + pos)
    if b.u(1):  # long_term_ref_pics_present
        for _ in range(b.ue()):
            b.u(log2_poc)
            b.u(1)
    b.u(1)
    b.u(1)
    if b.u(1):
        _vui_signal(b, out)
    else:
        out['full_range'] = None
    out['interlaced'] = False
    return out


def parse_sample_entry(entry):
    """entry: full visual sample entry bytes (size+fourcc+...)."""
    info = {}
    # child boxes start after the 86-byte visual sample entry header
    pos = 8 + 78
    while pos + 8 <= len(entry):
        size, typ = struct.unpack('>I4s', entry[pos:pos + 8])
        if size < 8:
            break
        body = entry[pos + 8:pos + size]
        try:
            if typ == b'avcC':
                nsps = body[5] & 0x1F
                if nsps:
                    ln = struct.unpack('>H', body[6:8])[0]
                    info.update(parse_avc_sps(body[8:8 + ln]))
            elif typ == b'hvcC':
                q = 23
                for _ in range(body[22]):
                    ntype = body[q] & 0x3F
                    cnt = struct.unpack('>H', body[q + 1:q + 3])[0]
                    q += 3
                    for _ in range(cnt):
                        ln = struct.unpack('>H', body[q:q + 2])[0]
                        if ntype == 33 and 'profile' not in info:
                            info.update(parse_hevc_sps(body[q + 2:q + 2 + ln]))
                        q += 2 + ln
            elif typ == b'colr' and body[:4] in (b'nclx', b'nclc'):   # nclc: QuickTime, without the range bit
                p, t, m = struct.unpack('>HHH', body[4:10])
                info['colr_primaries'] = PRIMARIES.get(p, str(p))
                info['colr_transfer'] = TRANSFER.get(t, str(t))
                info['colr_matrix'] = MATRIX.get(m, str(m))
                if body[:4] == b'nclx':
                    info['colr_full_range'] = bool(body[10] >> 7)
            elif typ == b'logs':   # Apple: the log profile by name, e.g. com.apple.apple-wide-gamut.apple-log
                info['log_profile'] = body.split(b'\0')[0].decode('ascii', 'replace')
        except (IndexError, ValueError, struct.error) as exc:
            info['parse_error'] = '%s: %s' % (typ.decode('latin1'), exc)
        pos += size
    return info


MPEG2_FPS = {1: 24000 / 1001, 2: 24.0, 3: 25.0, 4: 30000 / 1001, 5: 30.0, 6: 50.0, 7: 60000 / 1001, 8: 60.0}


def find_mpeg_video(data):
    """MPEG-1/2 sequence header (00 00 01 B3) and its sequence display extension, from the first bytes of a stream."""
    i = data.find(b'\x00\x00\x01\xb3')
    if i < 0 or i + 8 > len(data):
        return {}
    w = (data[i + 4] << 4) | (data[i + 5] >> 4)
    h = ((data[i + 5] & 15) << 8) | data[i + 6]
    info = {'codec': 'MPEG-2 Video', 'width': w, 'height': h, 'chroma': '4:2:0', 'bit_depth': 8,
            'fps': MPEG2_FPS.get(data[i + 7] & 15)}
    j = data.find(b'\x00\x00\x01\xb5', i)
    while 0 <= j < len(data) - 8:   # extension: id 2 = sequence display, with an optional colour description
        if data[j + 4] >> 4 == 2 and data[j + 4] & 1:
            info['vui_primaries'] = PRIMARIES.get(data[j + 5], str(data[j + 5]))
            info['vui_transfer'] = TRANSFER.get(data[j + 6], str(data[j + 6]))
            info['vui_matrix'] = MATRIX.get(data[j + 7], str(data[j + 7]))
            break
        if data[j + 4] >> 4 == 1:   # sequence extension: progressive_sequence bit
            info['interlaced'] = not (data[j + 5] >> 3) & 1
        j = data.find(b'\x00\x00\x01\xb5', j + 4)
    return info
