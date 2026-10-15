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
    m = Image.fromarray((mask * 255).astype(np.uint8))
    from PIL import ImageFilter
    m = np.asarray(m.filter(ImageFilter.MaxFilter(2 * grow + 1))) > 0
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

def write_lists(boxes, size):
    W, H = size
    lines = ["/* Made by tools/make_parts.py: where each part of Groucho's face lay on the sheet,",
             " * in its pixels, so that the parts go back where they were drawn. */",
             f"#define SHEET_W {W}", f"#define SHEET_H {H}",
             "typedef struct { int x0, y0, x1, y1; } PartBox;"]
    for name, (x0, y0, x1, y1) in sorted(boxes.items()):
        lines.append(f"static const PartBox BOX_{name} = {{{x0}, {y0}, {x1}, {y1}}};")
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
    subprocess.run(["magick", f"{SRC}/02-backdrop.jpeg", "-resize", "2560x", "-quality", "88", f"{OUT}/backdrop.jpg"], check=True)
    print("backdrop.jpg")
    write_lists(boxes, size)

if __name__ == "__main__":
    main()
