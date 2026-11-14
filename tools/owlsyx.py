#!/usr/bin/env python3
"""Pack an OWL patch binary into a .syx file, or unpack one, the way
OwlProgram's FirmwareSender does (`FirmwareSender -d 0 -in patch.bin -save patch.syx`).

  owlsyx.py pack   patch.bin  patch.syx
  owlsyx.py unpack patch.syx  patch.bin     (checks the CRC)

Format: sysex messages F0 7D 52 10 <index> ... F7, where <index> and every int are
4 bytes big-endian packed into 5 sysex bytes. Message 0 carries the length, the data
messages carry 210 bytes each (packed 7-to-8), the last one the CRC32 of the data.
"""
import sys
import zlib

HEADER = bytes([0x7D, 0x52, 0x10])  # manufacturer, omni device, firmware upload
BLOCK = (248 - 8) * 7 // 8           # 210 data bytes per message


def to_sysex(data):
    out = bytearray()
    for i in range(0, len(data), 7):
        chunk = data[i:i + 7]
        msbs = 0
        for j, c in enumerate(chunk):
            msbs |= (c >> 7) << j
        out.append(msbs)
        out.extend(c & 0x7F for c in chunk)
    return bytes(out)


def from_sysex(sx):
    out = bytearray()
    for i in range(0, len(sx), 8):
        bits = sx[i]
        for c in sx[i + 1:i + 8]:
            out.append(c | ((bits & 1) << 7))
            bits >>= 1
    return bytes(out)


def enc_int(v):
    return to_sysex(v.to_bytes(4, 'big'))


def pack(binary):
    msgs = []
    idx = 0

    def msg(payload):
        nonlocal idx
        msgs.append(b'\xf0' + HEADER + enc_int(idx) + payload + b'\xf7')
        idx += 1

    msg(enc_int(len(binary)))
    for i in range(0, len(binary), BLOCK):
        msg(to_sysex(binary[i:i + BLOCK]))
    msg(enc_int(zlib.crc32(binary) & 0xFFFFFFFF))
    return b''.join(msgs)


def unpack(syx):
    msgs = [m[1:] for m in syx.split(b'\xf7') if m.startswith(b'\xf0')]
    for m in msgs:
        if m[:3] != HEADER:
            raise ValueError('not an OWL firmware upload message')
    body = [m[8:] for m in msgs]  # strip header + index
    size = int.from_bytes(from_sysex(body[0]), 'big')
    data = b''.join(from_sysex(b) for b in body[1:-1])[:size]
    crc = int.from_bytes(from_sysex(body[-1]), 'big')
    if len(data) != size or zlib.crc32(data) & 0xFFFFFFFF != crc:
        raise ValueError('size or checksum mismatch')
    return data


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[1] not in ('pack', 'unpack'):
        sys.exit(__doc__)
    src = open(sys.argv[2], 'rb').read()
    out = pack(src) if sys.argv[1] == 'pack' else unpack(src)
    open(sys.argv[3], 'wb').write(out)
