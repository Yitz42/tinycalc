#!/usr/bin/env python3
"""Render sim draw-call output to PNG (approximate: a stand-in font for Gothic).
usage: sim ... frame | render.py out.png [scale]"""
import sys
from PIL import Image, ImageDraw, ImageFont

FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FONT_R = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"
CENTER = {28: 19, 24: 16, 18: 12, 14: 9}   # must match font_top() in main.c
PX = {28: 19, 24: 17, 18: 13, 14: 11}      # stand-in font pixel size

def hexc(s):
    v = int(s, 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)

def main():
    out = sys.argv[1]
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    S = 4  # supersample
    im = Image.new("RGB", (200 * S, 228 * S), "black")
    d = ImageDraw.Draw(im)
    fonts = {}
    def font(sz, bold):
        k = (sz, bold)
        if k not in fonts:
            fonts[k] = ImageFont.truetype(FONT_B if bold else FONT_R, PX[sz] * S)
        return fonts[k]
    inframe = False
    for line in sys.stdin.read().splitlines():
        if line == "FRAME":
            inframe = True; continue
        if line == "END":
            break
        if not inframe:
            continue
        p = line.split(" ")
        op = p[0]
        if op == "fill":
            x, y, w, h, r = map(int, p[1:6])
            d.rounded_rectangle([x * S, y * S, (x + w) * S - 1, (y + h) * S - 1], radius=r * S, fill=hexc(p[6]))
        elif op == "rrect":
            x, y, w, h, r = map(int, p[1:6])
            d.rounded_rectangle([x * S, y * S, (x + w) * S - 1, (y + h) * S - 1], radius=r * S,
                                outline=hexc(p[6]), width=int(p[7]) * S)
        elif op == "line":
            x0, y0, x1, y1 = map(int, p[1:5])
            d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=hexc(p[5]), width=int(p[6]) * S)
        elif op in ("fcircle", "circle"):
            x, y, r = map(int, p[1:4])
            box = [(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S]
            if op == "fcircle": d.ellipse(box, fill=hexc(p[4]))
            else: d.ellipse(box, outline=hexc(p[4]), width=S)
        elif op == "text":
            x, y, w, h, sz, al = map(int, p[1:7])
            col, bold = hexc(p[7]), p[8] == "B"
            t = " ".join(p[9:])
            if not t: continue
            f = font(sz, bold)
            bb = d.textbbox((0, 0), t, font=f)
            tw = bb[2] - bb[0]
            if al == 0: tx = x * S
            elif al == 1: tx = x * S + (w * S - tw) / 2
            else: tx = (x + w) * S - tw
            # vertical: centre of cap height at y + CENTER
            cap = d.textbbox((0, 0), "H", font=f)
            cy = (cap[1] + cap[3]) / 2
            ty = (y + CENTER[sz]) * S - cy
            d.text((tx - bb[0], ty), t, font=f, fill=col)
    im = im.resize((200, 228), Image.LANCZOS)
    if scale > 1:
        im = im.resize((200 * scale, 228 * scale), Image.NEAREST)
    im.save(out)

main()
