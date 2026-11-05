"""Builds assets/splash/splash.pak, the data of Osteon's start-up splash screen.

Inputs (assets/branding): the splash card image, the animated logo SVG (rendered frame by frame with headless Chrome).
Output: one pack file (embedded into Osteon.exe as a resource) holding
  - the card with the logo cut out (text is drawn live by the editor),
  - the PNG frames of the logo animation, cropped, with where they sit on the card,
  - an (unused, empty) link rectangle.
Run again after changing the version, the text or the artwork:  python tools/make_splash_assets.py
"""
import concurrent.futures, io, os, re, struct, subprocess, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "assets", "branding")
OUT = os.path.join(ROOT, "assets", "splash", "splash.pak")
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
FPS, DURATION = 30, 1.5          # the logo settles after about 1.35 s
RENDER = 1024                    # frames are rendered at this size, then stored at FRAME_SCALE of it
FRAME_SCALE = 0.75

def version():
    rc = open(os.path.join(ROOT, "src", "editor", "app.rc"), encoding="utf-8").read()
    m = re.search(r'VALUE "ProductVersion", "([^"]+)"', rc)
    return m.group(1) if m else "0.1.0"

def render_frames(tmp):
    svg = open(os.path.join(BRAND, "Osteon animated.svg"), encoding="utf-8").read()
    svg = svg.replace('height="100%" width="100%"', f'height="{RENDER}" width="{RENDER}"', 1)
    html = ('<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;background:transparent;overflow:hidden}</style></head><body>'
            + svg + '<script>const T=parseFloat(location.hash.slice(1)||"0");const s=document.querySelector("svg");s.pauseAnimations();s.setCurrentTime(T);</script></body></html>')
    page = os.path.join(tmp, "frame.html")
    open(page, "w", encoding="utf-8").write(html)
    n = int(DURATION * FPS) + 1
    def one(i):
        out = os.path.join(tmp, f"f{i:03d}.png")
        url = "file:///" + page.replace("\\", "/") + f"#{i / FPS:.4f}"
        subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--default-background-color=00000000",
                        f"--window-size={RENDER},{RENDER}", f"--screenshot={out}", url], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return Image.open(out).convert("RGBA")
    with concurrent.futures.ThreadPoolExecutor(4) as ex:
        return list(ex.map(one, range(n)))

def bright_bbox(img_rgba, alpha_min=200, bright_min=600):
    a = np.array(img_rgba)
    m = (a[..., 3] >= alpha_min) & (a[..., :3].astype(int).sum(axis=2) > bright_min)
    ys, xs = np.where(m)
    return xs.min(), ys.min(), xs.max(), ys.max()

def diffuse_fill(img, mask, iters=400):
    """Fills the masked pixels from their surroundings (the card behind the logo is a smooth gradient)."""
    a = np.array(img).astype(np.float32)
    m = mask[..., None]
    a[mask] = a[~mask].mean(axis=0)
    for _ in range(iters):
        b = (np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 4
        a = np.where(m, b, a)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8))

def font(name, size):
    return ImageFont.truetype(os.path.join(r"C:\Windows\Fonts", name), size)

def fit_font(name, text, target_w):
    lo, hi = 8, 400
    while hi - lo > 1:
        mid = (lo + hi) // 2
        w = ImageDraw.Draw(Image.new("L", (10, 10))).textbbox((0, 0), text, font=font(name, mid))[2]
        lo, hi = (mid, hi) if w <= target_w else (lo, mid)
    return font(name, lo)

def globe(d, cx, cy, r, color):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color, width=max(2, r // 9))
    d.ellipse([cx - r * 0.45, cy - r, cx + r * 0.45, cy + r], outline=color, width=max(1, r // 12))
    d.line([cx, cy - r, cx, cy + r], fill=color, width=max(1, r // 12))
    d.line([cx - r, cy, cx + r, cy], fill=color, width=max(1, r // 12))
    for k in (-0.5, 0.5):
        y = cy + k * r
        half = (r * r - (k * r) ** 2) ** 0.5
        d.line([cx - half, y, cx + half, y], fill=color, width=max(1, r // 14))

def main():
    ver = version()
    # The blank card supplied for the splash; the reference card (logo painted in) tells where the logo goes.
    card_full = Image.open(os.path.join(BRAND, "splash_background.png")).convert("RGBA")
    ref_full = Image.open(os.path.join(BRAND, "Splash screen.png")).convert("RGBA")
    al = np.array(card_full)[..., 3]
    ys, xs = np.where(al > 250)
    cx0, cy0, cx1, cy1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    clean = card_full.crop((cx0, cy0, cx1, cy1))
    W, H = clean.size
    print("card", W, H, "version", ver)

    with tempfile.TemporaryDirectory() as tmp:
        frames = render_frames(tmp)
    # Place the animation so its settled logo lies exactly where the reference card has its logo (card-local pixels).
    lx0, ly0, lx1, ly1 = bright_bbox(ref_full.crop((cx0, cy0, cx1, cy1)))
    fx0, fy0, fx1, fy1 = bright_bbox(frames[-1])
    scale = (lx1 - lx0) / (fx1 - fx0)                      # card pixels per rendered pixel
    print("logo in card", (lx0, ly0, lx1, ly1), "frame logo", (fx0, fy0, fx1, fy1), "scale", scale)

    # No text is baked in: the title line, version, tagline and loader are drawn live by the editor (so they can be translated).
    link = (0, 0, 0, 0)  # no clickable area on the card

    # Frames: crop to the union of what is drawn, scale down for storage.
    union = None
    for fr in frames:
        bb = fr.getchannel("A").point(lambda v: 255 if v > 2 else 0).getbbox()
        if bb:
            union = bb if union is None else (min(union[0], bb[0]), min(union[1], bb[1]), max(union[2], bb[2]), max(union[3], bb[3]))
    blobs = []
    for fr in frames:
        c = fr.crop(union)
        c = c.resize((max(1, round(c.width * FRAME_SCALE)), max(1, round(c.height * FRAME_SCALE))), Image.LANCZOS)
        b = io.BytesIO(); c.save(b, "PNG", optimize=True); blobs.append(b.getvalue())
    fw, fh = round((union[2] - union[0]) * FRAME_SCALE), round((union[3] - union[1]) * FRAME_SCALE)
    # Where the cropped frame's top-left lands on the card, and how many card pixels one stored pixel covers.
    place_x = lx0 - (fx0 - union[0]) * scale
    place_y = ly0 - (fy0 - union[1]) * scale
    card_per_px = scale / FRAME_SCALE
    print("frames", len(blobs), "size", fw, fh, "bytes", sum(map(len, blobs)), "placed", place_x, place_y, "per px", card_per_px)

    cb = io.BytesIO(); clean.save(cb, "PNG", optimize=True)
    card_png = cb.getvalue()
    head = struct.pack("<4sIIIIIf", b"OSPK", 1, W, H, len(blobs), 0, float(FPS))
    head += struct.pack("<fff", float(place_x), float(place_y), float(card_per_px))
    head += struct.pack("<ffff", *map(float, link))
    entries = [card_png] + blobs
    table_size = 8 * len(entries)
    off = len(head) + table_size
    table = b""
    for e in entries:
        table += struct.pack("<II", off, len(e)); off += len(e)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "wb").write(head + table + b"".join(entries))
    print("wrote", OUT, os.path.getsize(OUT), "bytes")
    # A preview of the finished splash with the logo settled, for a quick look.
    prev = clean.copy()
    last = Image.open(io.BytesIO(blobs[-1])).convert("RGBA")
    last = last.resize((round(last.width * card_per_px), round(last.height * card_per_px)), Image.LANCZOS)
    prev.alpha_composite(last, (round(place_x), round(place_y)))
    prev.save(os.path.join(ROOT, "assets", "splash", "preview.png"))

if __name__ == "__main__":
    main()
