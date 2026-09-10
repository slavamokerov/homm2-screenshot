#!/usr/bin/env python3
"""Minimal PNG decoder + image alignment/diff for the render-vs-engine harness.

No external deps (only zlib/struct). Supports the PNG variants a screenshot or
our ARGB render will produce: color types 0,2,3,4,6, bit depths 1/2/4/8/16,
non-interlaced (Adam7 screenshots are not produced by the game or Qt).

Usage:
  pngdiff.py <imgA> <imgB>          # full-image pixel diff (same dims)
  pngdiff.py --match <big> <small>  # find 'small' inside 'big' (offset + diff)
  pngdiff.py <imgA> <imgB> --crop x,y,w,h   # crop A before comparing

Outputs: match offset (dx,dy), diff pixel count, % differing, mean abs diff,
and a list of hot-spot bounding boxes where differences cluster.
"""

import sys
import zlib
import struct


def u32(b, o):
    return struct.unpack(">I", b[o:o + 4])[0]


class PNG:
    def __init__(self, pixels, width, height):
        self.px = pixels  # list of (r,g,b,a)
        self.width = width
        self.height = height


def decode(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "%s: not a PNG" % path

    pos = 8
    width = height = None
    bitdepth = colortype = None
    idat = b""
    plte = b""
    trns = None
    while pos < len(data):
        ln = u32(data, pos)
        typ = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(">IIBB", chunk[:10])
            interlace = chunk[12]
            assert interlace == 0, "interlaced PNG not supported"
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"PLTE":
            plte = chunk
        elif typ == b"tRNS":
            trns = chunk
        pos += 12 + ln

    assert width is not None, "no IHDR"
    raw = zlib.decompress(idat)

    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colortype]
    bpp = max(1, channels * bitdepth // 8)
    stride = (width * channels * bitdepth + 7) // 8
    # bytes per row in the filtered stream is 1 filter byte + stride
    rows = []
    roff = 0
    prev = bytearray(stride)
    for y in range(height):
        ftype = raw[roff]
        roff += 1
        line = bytearray(raw[roff:roff + stride])
        roff += stride
        if ftype == 1:  # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa = abs(p - a)
                pb = abs(p - b)
                pc = abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        elif ftype != 0:
            raise ValueError("bad filter %d" % ftype)
        rows.append(bytes(line))
        prev = line

    # Convert to RGBA rows based on colortype/bitdepth.
    px = []
    if bitdepth == 8:
        for y in range(height):
            row = rows[y]
            i = 0
            for x in range(width):
                if colortype == 2:
                    r, g, b = row[i], row[i + 1], row[i + 2]
                    i += 3
                    px.append((r, g, b, 255))
                elif colortype == 6:
                    r, g, b, a = row[i], row[i + 1], row[i + 2], row[i + 3]
                    i += 4
                    px.append((r, g, b, a))
                elif colortype == 3:
                    idx = row[i]; i += 1
                    p = idx * 3
                    r, g, b = plte[p], plte[p + 1], plte[p + 2]
                    a = trns[idx] if (trns is not None and idx < len(trns)) else 255
                    px.append((r, g, b, a))
                elif colortype == 0:
                    v = row[i]; i += 1
                    px.append((v, v, v, 255))
                elif colortype == 4:
                    gv, a = row[i], row[i + 1]; i += 2
                    px.append((gv, gv, gv, a))
    elif bitdepth == 16:
        # only colortype 0/2/6 expected; take high byte
        for y in range(height):
            row = rows[y]
            i = 0
            for x in range(width):
                if colortype == 2:
                    r, g, b = row[i], row[i + 2], row[i + 4]
                    i += 6
                    px.append((r, g, b, 255))
                elif colortype == 6:
                    r, g, b, a = row[i], row[i + 2], row[i + 4], row[i + 6]
                    i += 8
                    px.append((r, g, b, a))
                else:
                    v = row[i]; i += 2
                    px.append((v, v, v, 255))
    else:
        # palette/grayscale <=4bpp: unpack pixels into groups
        for y in range(height):
            row = rows[y]
            if colortype == 3:
                per = 8 // bitdepth
                mask = (1 << bitdepth) - 1
                for x in range(width):
                    byte = row[x // per]
                    shift = 8 - bitdepth * ((x % per) + 1)
                    idx = (byte >> shift) & mask
                    p = idx * 3
                    px.append((plte[p], plte[p + 1], plte[p + 2], 255))
            else:
                per = 8 // bitdepth
                mask = (1 << bitdepth) - 1
                for x in range(width):
                    byte = row[x // per]
                    shift = 8 - bitdepth * ((x % per) + 1)
                    v = (byte >> shift) & mask
                    v = v * 255 // mask
                    px.append((v, v, v, 255))

    return PNG(px, width, height)


def sub(img, x, y, w, h):
    out = PNG([], w, h)
    for yy in range(h):
        base = (y + yy) * img.width + x
        out.px.extend(img.px[base:base + w])
    return out


def resize(img, dw, dh):
    """Bilinear resize to dw x dh (used to match render scale vs. a screenshot)."""
    if dw == img.width and dh == img.height:
        return img
    sx = img.width / dw
    sy = img.height / dh
    out = PNG([], dw, dh)
    for y in range(dh):
        fy = (y + 0.5) * sy - 0.5
        y0 = int(fy) if fy >= 0 else 0
        y1 = min(img.height - 1, y0 + 1)
        wy = fy - y0
        if wy < 0:
            wy = 0.0
        for x in range(dw):
            fx = (x + 0.5) * sx - 0.5
            x0 = int(fx) if fx >= 0 else 0
            x1 = min(img.width - 1, x0 + 1)
            wx = fx - x0
            if wx < 0:
                wx = 0.0
            p00 = img.px[y0 * img.width + x0]
            p10 = img.px[y0 * img.width + x1]
            p01 = img.px[y1 * img.width + x0]
            p11 = img.px[y1 * img.width + x1]
            r = (p00[0] * (1 - wx) + p10[0] * wx) * (1 - wy) + (p01[0] * (1 - wx) + p11[0] * wx) * wy
            g = (p00[1] * (1 - wx) + p10[1] * wx) * (1 - wy) + (p01[1] * (1 - wx) + p11[1] * wx) * wy
            b = (p00[2] * (1 - wx) + p10[2] * wx) * (1 - wy) + (p01[2] * (1 - wx) + p11[2] * wx) * wy
            a = (p00[3] * (1 - wx) + p10[3] * wx) * (1 - wy) + (p01[3] * (1 - wx) + p11[3] * wx) * wy
            out.px.append((int(r), int(g), int(b), int(a)))
    return out


def gray(p):
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) // 1000


def mean_abs_diff(a, b, ax, ay, bx, by, w, h, step_gray=True):
    total = 0
    n = w * h
    for yy in range(h):
        ia = (ay + yy) * a.width + ax
        ib = (by + yy) * b.width + bx
        row = a.px[ia:ia + w]
        for x in range(w):
            pa = row[x]
            pb = b.px[ib + x]
            if step_gray:
                total += abs(gray(pa) - gray(pb))
            else:
                total += (abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2]))
    return total / n


def find_offset(big, small, step=2):
    """Find top-left placement of 'small' inside 'big'; returns (dx,dy,score)."""
    def downsample(img, k):
        w, h = img.width // k, img.height // k
        if w < 2 or h < 2:
            return img
        out = PNG([], w, h)
        px = img.px
        for y in range(h):
            base = (y * k) * img.width
            row = out.px
            for x in range(w):
                r = g = b = 0
                for yy in range(k):
                    for xx in range(k):
                        p = px[base + yy * img.width + x * k + xx]
                        r += p[0]; g += p[1]; b += p[2]
                n = k * k
                row.append((r // n, g // n, b // n, 255))
        return out

    def search(big, small, step):
        best = None
        for y in range(0, big.height - small.height + 1, step):
            for x in range(0, big.width - small.width + 1, step):
                score = mean_abs_diff(big, small, x, y, 0, 0, small.width, small.height)
                if best is None or score < best[2]:
                    best = (x, y, score)
        return best

    # Pyramid of scales: coarse (whole-image) to fine. Each finer scale refines
    # around the previous best, so per-level windows stay small.
    best = (0, 0, float("inf"))
    first = True
    for k in (16, 8, 4, 2, 1):
        if big.width // k < small.width // k + 2 or big.height // k < small.height // k + 2:
            continue
        bd = downsample(big, k)
        sd = downsample(small, k)
        limit_x = bd.width - sd.width
        limit_y = bd.height - sd.height
        cx, cy = best[0] // k, best[1] // k
        pad = 0 if first else 3
        first = False
        y0 = max(0, cy - pad); y1 = min(limit_y, cy + pad)
        x0 = max(0, cx - pad); x1 = min(limit_x, cx + pad)
        cand = None
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                score = mean_abs_diff(bd, sd, x, y, 0, 0, sd.width, sd.height)
                if cand is None or score < cand[2]:
                    cand = (x, y, score)
        if cand is None:
            continue
        best = (cand[0] * k, cand[1] * k, cand[2])
    return best


def diff_index(a, b):
    """Same-size diff; returns (count, total, mean, hot_regions)."""
    w = min(a.width, b.width)
    h = min(a.height, b.height)
    count = 0
    total = 0
    hots = []  # (x0,y0,x1,y1,mad)
    for y in range(h):
        ia = y * a.width
        ib = y * b.width
        for x in range(w):
            pa = a.px[ia + x]
            pb = b.px[ib + x]
            if pa != pb:
                count += 1
                total += gray(pa) in (0,) and 0 or abs(gray(pa) - gray(pb))
    mean = total / (w * h) if w * h else 0
    return count, total, mean, hots


def main():
    args = sys.argv[1:]
    match = False
    crop = None
    resize_wh = None
    imgs = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--match":
            match = True
        elif a == "--crop":
            crop = [int(v) for v in args[i + 1].split(",")]
            i += 1
        elif a == "--resize":
            resize_wh = [int(v) for v in args[i + 1].split(",")]
            i += 1
        else:
            imgs.append(a)
        i += 1

    if len(imgs) != 2:
        print("usage: pngdiff.py [-match] [-crop x,y,w,h] [-resize W,H] <A> <B>")
        sys.exit(2)

    A = decode(imgs[0])
    B = decode(imgs[1])
    if crop:
        A = sub(A, crop[0], crop[1], crop[2], crop[3])
    if resize_wh:
        A = resize(A, resize_wh[0], resize_wh[1])

    if match and (A.width < B.width or A.height < B.height):
        A, B = B, A
        print("note: swapped so 'big' contains 'small'")

    if match:
        big, small = A, B
        dx, dy, score = find_offset(big, small)
        print("match: small %dx%d placed at (%d,%d) mad=%.2f" % (small.width, small.height, dx, dy, score))
        # crop big to the matched region and diff vs small
        A = sub(big, dx, dy, small.width, small.height)
        B = small

    if A.width != B.width or A.height != B.height:
        print("size mismatch A=%dx%d B=%dx%d" % (A.width, A.height, B.width, B.height))
        sys.exit(1)

    count, total, mean, _ = diff_index(A, B)
    pct = 100.0 * count / (A.width * A.height) if A.width * A.height else 0
    print("size %dx%d" % (A.width, A.height))
    print("differing pixels: %d (%.3f%%)" % (count, pct))
    print("mean abs gray diff: %.2f" % mean)


if __name__ == "__main__":
    main()
