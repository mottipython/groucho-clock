#!/usr/bin/env python3
"""Cut the Gemini sheets in assets/src into the clock's parts, in assets/art, and
write src/art.h (every picture, for the program to build in).

  01-face.jpeg      Groucho's head with blank eyes, and each part on its own: the
                    grin, the glasses, the eyebrows, a pupil, the cigar at four
                    lengths and a wisp of smoke. Each part is found by where it
                    lies on the sheet (PARTS), so the sheet can be redrawn.
  02-backdrop.jpeg  the theatre, as it is
  03-gummo.jpeg     Gummo peeking in, four frames left to right

The magenta is keyed out and its colour taken off the edges. Each part is cropped
to itself; tools/make_parts.py also prints where each part was on the sheet, which
the program uses to put them back (src/parts.h).

    tools/make_parts.py
"""
import os, subprocess
import numpy as np
from PIL import Image

SRC, OUT = "assets/src", "assets/art"

# the face sheet's parts: name and where its middle is, as part of the sheet
PARTS = {
    "face": (0.27, 0.40), "grin": (0.62, 0.11), "glasses": (0.86, 0.11),
    "brow_l": (0.63, 0.27), "brow_r": (0.85, 0.27), "pupil": (0.63, 0.37),
    "cigar0": (0.74, 0.46), "cigar1": (0.74, 0.57), "cigar2": (0.74, 0.68),
    "cigar3": (0.65, 0.83), "smoke": (0.89, 0.84),
}

def key(img):
    """Magenta background to alpha, and take its colour cast off the edges."""
    r, g, b = img[..., 0].copy(), img[..., 1].copy(), img[..., 2].copy()
    spill = np.minimum(r, b) - g
    cut = np.clip(spill, 0, None) * 0.92
    r, b = r - cut, b - cut
    alpha = np.clip(1.0 - (spill - 24) / 70.0, 0, 1)
    return np.dstack([r, g, b, alpha * 255]).clip(0, 255).astype(np.uint8)

def blobs(mask, grow=6):
    """The separate shapes on the sheet (each grown a little first, so a brow's
    loose hairs stay with it), as a label image and a count."""
    m = mask
    if grow > 0:                             # (a 1-pixel MaxFilter crashes Pillow)
        from PIL import ImageFilter
        m = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * grow + 1))) > 0
    lab = np.zeros(m.shape, np.int32); n = 0
    h, w = m.shape
    for y0, x0 in zip(*np.nonzero(m)):
        if lab[y0, x0]: continue
        n += 1; stack = [(y0, x0)]; lab[y0, x0] = n
        while stack:
            y, x = stack.pop()
            for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not lab[ny, nx]:
                    lab[ny, nx] = n; stack.append((ny, nx))
    return lab, n

def save(rgba, path):
    Image.fromarray(rgba, "RGBA").save(path, optimize=True)

def cut_sheet(path, names_at):
    """Each shape on the sheet, named for the PARTS place nearest its middle."""
    img = np.asarray(Image.open(path).convert("RGB")).astype(float)
    k = key(img); H, W = k.shape[:2]
    lab, n = blobs(k[..., 3] > 128)
    found = {}
    for i in range(1, n + 1):
        ys, xs = np.nonzero(lab == i)
        if len(ys) < 400: continue                       # specks
        cx, cy = xs.mean() / W, ys.mean() / H
        name = min(names_at, key=lambda nm: (names_at[nm][0] - cx) ** 2 + (names_at[nm][1] - cy) ** 2)
        d = (names_at[name][0] - cx) ** 2 + (names_at[name][1] - cy) ** 2
        if name in found and found[name][0] <= d: continue
        found[name] = (d, i, xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    boxes = {}
    for name, (d, i, x0, y0, x1, y1) in found.items():
        part = k[y0:y1, x0:x1].copy()
        part[..., 3] = np.where(lab[y0:y1, x0:x1] == i, part[..., 3], 0)
        save(part, f"{OUT}/{name}.png")
        boxes[name] = (x0, y0, x1, y1)
        print(f"{name}.png {x1 - x0}x{y1 - y0} at {x0},{y0}")
    missing = set(names_at) - set(found)
    if missing: print("MISSING:", ", ".join(sorted(missing)))
    return boxes, (W, H)

def brothers():
    """Chico, Harpo and Zeppo, a row each, four frames across: the biggest shape in
    each cell of the 4 x 3 sheet."""
    img = np.asarray(Image.open(f"{SRC}/04-brothers.jpeg").convert("RGB")).astype(float)
    k = key(img); H, W = k.shape[:2]
    lab, n = blobs(k[..., 3] > 128)
    sizes = np.bincount(lab.ravel())
    for r, name in enumerate(("chico", "harpo", "zeppo")):
        for c in range(4):
            cell = lab[r * H // 3:(r + 1) * H // 3, c * W // 4:(c + 1) * W // 4]
            ids, cnt = np.unique(cell[cell > 0], return_counts=True)
            i = ids[np.argmax(cnt)]
            ys, xs = np.nonzero(lab == i)
            a, b, cc, d = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
            part = k[b:d, a:cc].copy()
            part[..., 3] = np.where(lab[b:d, a:cc] == i, part[..., 3], 0)
            save(part, f"{OUT}/{name}{c}.png")
            print(f"{name}{c}.png {cc - a}x{d - b}")

def gummo():
    img = np.asarray(Image.open(f"{SRC}/03-gummo.jpeg").convert("RGB")).astype(float)
    k = key(img); H, W = k.shape[:2]
    lab, n = blobs(k[..., 3] > 128)
    shapes = []
    for i in range(1, n + 1):
        ys, xs = np.nonzero(lab == i)
        if len(ys) > 5000: shapes.append((xs.min(), i, xs, ys))
    for f, (x0, i, xs, ys) in enumerate(sorted(shapes, key=lambda s: s[0])[:4]):
        a, b, c, d = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
        part = k[b:d, a:c].copy()
        part[..., 3] = np.where(lab[b:d, a:c] == i, part[..., 3], 0)
        save(part, f"{OUT}/gummo{f}.png")
        print(f"gummo{f}.png {c - a}x{d - b}")

STACHE_AT = (478, 640)           # a point in his moustache, on the face picture
GLASSES_BOX = (1510, 150, 2000, 335)      # where the glasses are on the face sheet
def glasses():
    """The glasses are a thin dark wire on the magenta, too close to it for the key:
    cut them out by darkness instead (anything well darker than the background), and
    keep the silver of their arms."""
    img = np.asarray(Image.open(f"{SRC}/01-face.jpeg").convert("RGB")).astype(float)
    x0, y0, x1, y1 = GLASSES_BOX
    c = img[y0:y1, x0:x1]
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    ink = np.clip((210 - (r + b) / 2) / 90, 0, 1)
    silver = (np.abs(r - g) < 35) & (np.abs(b - g) < 35) & (g > 120)
    a = np.where(silver, 1.0, ink)
    rgb = np.where(silver[..., None], c, np.array([38, 32, 36]))
    out = np.dstack([rgb, a * 255]).astype(np.uint8)
    ys, xs = np.nonzero(out[..., 3] > 60)
    out = out[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    save(out, f"{OUT}/glasses.png")
    print(f"glasses.png {out.shape[1]}x{out.shape[0]} (cut by darkness)")

def stache():
    """His moustache, cut out of the face picture as a piece of its own, so it can be
    laid back over the grin: the dark shape that holds STACHE_AT, with its outline."""
    from PIL import ImageFilter
    face = np.asarray(Image.open(f"{OUT}/face.png").convert("RGBA")).copy()
    dark = (face[..., :3].max(-1) < 95) & (face[..., 3] > 128)
    lab, n = blobs(dark, grow=0)
    i = lab[STACHE_AT[1], STACHE_AT[0]]
    if not i: print("MISSING: stache"); return
    # only its thick body, not the thin lines of his nose and smile that touch it:
    # open it (shrink, then grow back), and take its ink outline with it
    body = Image.fromarray(((lab == i) * 255).astype(np.uint8))
    # close it (fill the shine on the greasepaint), then open it (drop the nose's
    # lines), then grow a little for its ink edge, and smooth the outline
    body = body.filter(ImageFilter.MaxFilter(11)).filter(ImageFilter.MinFilter(11))
    body = body.filter(ImageFilter.MinFilter(17)).filter(ImageFilter.MaxFilter(17)).filter(ImageFilter.MaxFilter(5))
    body = body.filter(ImageFilter.GaussianBlur(3)).point(lambda v: 255 if v > 128 else 0)
    m = (np.asarray(body) > 0) & (face[..., 3] > 0)
    # only the rows where it is nearly full width: not the nostrils above, nor the
    # smile lines below
    widths = m.sum(1)
    m[widths < 0.75 * widths.max()] = False
    ys, xs = np.nonzero(m)
    x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    part = face[y0:y1, x0:x1].copy()
    part[..., 3] = np.where(m[y0:y1, x0:x1], part[..., 3], 0)
    save(part, f"{OUT}/stache.png")
    print(f"stache.png {x1 - x0}x{y1 - y0} at {x0},{y0} on the face")
    return x0, y0

def ash():
    """The last cigar split in two, so its long ash can break off and fall: the grey
    ash with its outline, and the stub left behind."""
    from PIL import ImageFilter
    c = np.asarray(Image.open(f"{OUT}/cigar3.png").convert("RGBA")).copy()
    r, g, b = (c[..., i].astype(int) for i in range(3))
    grey = (abs(r - g) < 22) & (abs(g - b) < 22) & (r > 110) & (c[..., 3] > 128)
    h, w = grey.shape
    xx = np.arange(w)[None, :].repeat(h, 0); yy = np.arange(h)[:, None].repeat(w, 1)
    brown = (r - b > 40) & (c[..., 3] > 128)
    CUT = 206                                             # where the stub ends and the ash begins
    m = (xx >= CUT - 8) | ((yy > 134) & (xx > 185))       # the ash, with its outline, but none of the stub
    m &= ~(brown & (xx < CUT + 4)) & (c[..., 3] > 0)
    a = c.copy(); a[..., 3] = np.where(m, a[..., 3], 0)
    stub = c.copy(); stub[..., 3] = np.where((xx < CUT) & (yy <= 128), stub[..., 3], 0)
    ys, xs = np.nonzero(a[..., 3] > 0)
    x0, y0 = xs.min(), ys.min()
    save(a[y0:, x0:], f"{OUT}/ash.png")
    save(stub, f"{OUT}/stub.png")
    print(f"ash.png at {x0},{y0} on the last cigar; stub.png")
    return x0, y0

def write_lists(boxes, size, st, ash_at):
    W, H = size
    lines = ["/* Made by tools/make_parts.py: where each part of Groucho's face lay on the sheet,",
             " * in its pixels, so that the parts go back where they were drawn. */",
             f"#define SHEET_W {W}", f"#define SHEET_H {H}",
             "typedef struct { int x0, y0, x1, y1; } PartBox;"]
    for name, (x0, y0, x1, y1) in sorted(boxes.items()):
        lines.append(f"static const PartBox BOX_{name} = {{{x0}, {y0}, {x1}, {y1}}};")
    lines.append(f"#define ASH_ON_CIGAR_X {ash_at[0]}\n#define ASH_ON_CIGAR_Y {ash_at[1]}   /* where the ash was on the last cigar */")
    lines.append(f"#define STACHE_ON_FACE_X {st[0]}\n#define STACHE_ON_FACE_Y {st[1]}   /* where the moustache piece came from on the face */")
    open("src/parts.h", "w").write("\n".join(lines) + "\n")
    files = sorted(f for f in os.listdir(OUT) if f.endswith((".png", ".jpg")))
    items = [f'X({os.path.splitext(f)[0].replace("-", "_")}, "{f}")' for f in files]
    lines = ["/* Made by tools/make_parts.py: every picture in assets/art. */", "#define SPRITES \\"]
    for i in range(0, len(items), 4):
        lines.append("    " + " ".join(items[i:i + 4]) + (" \\" if i + 4 < len(items) else ""))
    open("src/art.h", "w").write("\n".join(lines) + "\n")
    print(f"src/art.h: {len(files)} pictures")

def main():
    os.makedirs(OUT, exist_ok=True)
    boxes, size = cut_sheet(f"{SRC}/01-face.jpeg", PARTS)
    gummo()
    brothers()
    subprocess.run(["magick", f"{SRC}/02-backdrop.jpeg", "-resize", "2560x", "-quality", "88", f"{OUT}/backdrop.jpg"], check=True)
    print("backdrop.jpg")
    glasses()
    st = stache()
    write_lists(boxes, size, st, ash())

if __name__ == "__main__":
    main()
