#!/usr/bin/env python3
"""
Builds the editor artwork for J60 Chorus from five renders of the same scene.

The renders must share camera, lights, resolution and noise seed, and differ
only in the control being changed:

    python tools/make_sprites.py \
        --mono-off   renders/mono_off.png    (slider on MONO,   nothing pressed)
        --stereo-off renders/stereo_off.png  (slider on STEREO, nothing pressed)
        --chorus1    renders/chorus1.png     (button I pressed,  LED I lit)
        --chorus2    renders/chorus2.png     (button II pressed, LED II lit)
        --both       renders/both.png        (both pressed, both LEDs lit)

What it does:
  * Uses the mono render as the background (assets/bg.png).
  * Finds which pixels change for each control, widens that area and feathers
    its edge into an alpha map, then writes small RGBA sprites for each state.
  * Writes EditorLayout.h with the sprite positions and click areas.
  * Rebuilds every state you rendered from background + sprites and reports
    how close each rebuild is to the real render.

Needs: Python 3, numpy, scipy, Pillow.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

ROOT = Path(__file__).resolve().parent.parent

# --- Tuning ------------------------------------------------------------------
THRESH = 3     # max-channel difference that counts as "changed"
DILATE = 8     # pixels the mask is widened beyond the changed area
FEATHER = 3.0  # gaussian sigma of the soft edge, in pixels

# Differences are only searched for inside these boxes (x0, x1, y0, y1, in
# render pixels), so render noise elsewhere (e.g. the mic grille) can never
# leak into a sprite.
ZONES = {
    "slider":  (905, 1030, 318, 376),
    "led1":    (888, 940, 378, 406),
    "led2":    (996, 1048, 378, 406),
    "buttons": (845, 1095, 435, 585),
}

# Hand-placed click areas, in render pixels (measured by eye from the render).
SLIDER_HIT = (855, 1105, 325, 380)   # x0, x1, y0, y1; includes MONO/STEREO labels
FOOT = (968, 940, 55)                # centre x, centre y, radius

# Area of the pedal used when checking the rebuilt renders.
PEDAL_BOX = (690, 1200, 180, 1060)
# -----------------------------------------------------------------------------


def load(path):
    return np.asarray(Image.open(path).convert("RGB"))


def changed_pixels(base, state, zone, thresh=THRESH):
    x0, x1, y0, y1 = zone
    diff = np.abs(base[y0:y1, x0:x1].astype(np.int16) - state[y0:y1, x0:x1].astype(np.int16)).max(axis=2)
    out = np.zeros(base.shape[:2], bool)
    out[y0:y1, x0:x1] = diff > thresh
    return out


def make_alpha(changed):
    """Widened, feathered alpha map (float, 0..1) around a boolean 'changed' image."""
    solid = ndimage.binary_closing(changed, structure=np.ones((3, 3), bool), iterations=2) | changed
    solid = ndimage.binary_fill_holes(solid)   # e.g. the flat face of a button whose edges changed
    dist = ndimage.distance_transform_edt(~solid)
    widened = (dist <= DILATE).astype(np.float32)
    alpha = ndimage.gaussian_filter(widened, FEATHER)
    return np.maximum(alpha, solid.astype(np.float32)).clip(0.0, 1.0)


def bbox(mask, level=0.0):
    ys, xs = np.where(mask > level)
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1   # x0, y0, x1, y1 (exclusive)


def make_sprite(state, alpha, rect):
    x0, y0, x1, y1 = rect
    a = np.rint(alpha[y0:y1, x0:x1] * 255.0).astype(np.uint8)
    return Image.fromarray(np.dstack([state[y0:y1, x0:x1], a]), "RGBA")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ("mono-off", "stereo-off", "chorus1", "chorus2", "both"):
        ap.add_argument("--" + name, required=True)
    ap.add_argument("--preview", default=str(ROOT / "tools" / "mask_preview.png"))
    args = ap.parse_args()

    mono = load(args.mono_off)
    stereo = load(args.stereo_off)
    c1 = load(args.chorus1)
    c2 = load(args.chorus2)
    both = load(args.both)
    for name, im in (("stereo-off", stereo), ("chorus1", c1), ("chorus2", c2), ("both", both)):
        if im.shape != mono.shape:
            sys.exit("%s has a different size from mono-off: %s vs %s" % (name, im.shape, mono.shape))
    height, width = mono.shape[:2]

    assets = ROOT / "assets"
    assets.mkdir(exist_ok=True)
    Image.fromarray(mono).save(assets / "bg.png", optimize=True)

    # control, search zone, states that define the mask, sprite name -> state image
    jobs = [
        ("slider",  "slider",  [stereo],        {"slider_stereo": stereo}),
        ("led1",    "led1",    [c1, both],      {"led1_on": c1}),
        ("led2",    "led2",    [c2, both],      {"led2_on": c2}),
        ("buttons", "buttons", [c1, c2, both],  {"btn_1": c1, "btn_2": c2, "btn_12": both}),
    ]

    rects, hit_boxes, alphas, sprites = {}, {}, {}, {}
    for control, zone, mask_states, files in jobs:
        changed = np.zeros((height, width), bool)
        strong = np.zeros((height, width), bool)   # clearly visible changes, used for click areas
        for st in mask_states:
            changed |= changed_pixels(mono, st, ZONES[zone])
            strong |= changed_pixels(mono, st, ZONES[zone], thresh=16)
        if not strong.any():
            sys.exit("No change found for '%s'. Are the renders the right ones?" % control)
        alpha = make_alpha(changed)
        rect = bbox(alpha, 0.004)
        rect = (max(rect[0], 0), max(rect[1], 0), min(rect[2], width), min(rect[3], height))
        rects[control] = rect
        hit_boxes[control] = bbox(strong)
        alphas[control] = alpha
        print("%-8s changed %5d px (visible box %s) -> sprite rect %s (%dx%d)" % (
            control, changed.sum(), hit_boxes[control], rect, rect[2] - rect[0], rect[3] - rect[1]))
        for fname, st in files.items():
            img = make_sprite(st, alpha, rect)
            img.save(assets / (fname + ".png"), optimize=True)
            sprites[fname] = (np.asarray(img).astype(np.float32), rect)

    # --- Layout header -------------------------------------------------------
    def r(x0, y0, x1, y1):
        return "{ %d, %d, %d, %d }" % (x0, y0, x1 - x0, y1 - y0)

    sx0, sy0, sx1, sy1 = hit_boxes["slider"]
    bx0, by0, bx1, by1 = hit_boxes["buttons"]
    mid = (bx0 + bx1) // 2
    hx0, hx1, hy0, hy1 = SLIDER_HIT
    header = """// Generated by tools/make_sprites.py - do not edit by hand.
// All values are in pixels of the %dx%d render.
#pragma once

namespace layout
{
constexpr int kImageW = %d;
constexpr int kImageH = %d;

struct Rect
{
    int x, y, w, h;

    constexpr bool contains (float px, float py) const noexcept
    {
        return px >= (float) x && px < (float) (x + w) && py >= (float) y && py < (float) (y + h);
    }
};

// Where each sprite is drawn.
constexpr Rect kSlider  = %s;
constexpr Rect kLed1    = %s;
constexpr Rect kLed2    = %s;
constexpr Rect kButtons = %s;

// Click areas.
constexpr Rect  kHitSlider  = %s;
constexpr float kSliderCentreX = %d.0f;   // click left of this = MONO, right = STEREO
constexpr Rect  kHitButton1 = %s;
constexpr Rect  kHitButton2 = %s;
constexpr float kFootX = %d.0f;
constexpr float kFootY = %d.0f;
constexpr float kFootR = %d.0f;
} // namespace layout
""" % (width, height, width, height,
       r(*rects["slider"]), r(*rects["led1"]), r(*rects["led2"]), r(*rects["buttons"]),
       r(hx0, hy0, hx1, hy1), (sx0 + sx1) // 2,
       r(bx0, by0, mid, by1), r(mid, by0, bx1, by1),
       FOOT[0], FOOT[1], FOOT[2])
    (ROOT / "EditorLayout.h").write_text(header)

    # --- Rebuild every rendered state and compare ----------------------------
    def composite(is_stereo, led1, led2, btn):
        out = mono.astype(np.float32).copy()

        def put(name):
            arr, (x0, y0, x1, y1) = sprites[name]
            a = arr[..., 3:4] / 255.0
            out[y0:y1, x0:x1] = out[y0:y1, x0:x1] * (1.0 - a) + arr[..., :3] * a

        if is_stereo:
            put("slider_stereo")
        if led1:
            put("led1_on")
        if led2:
            put("led2_on")
        if btn:
            put({1: "btn_1", 2: "btn_2", 3: "btn_12"}[btn])
        return np.clip(np.rint(out), 0, 255).astype(np.uint8)

    print()
    print("Rebuilt (background + sprites) vs the real render, pedal area only.")
    print("'raw' counts pixels off by more than 8 levels; 'visible' is the worst difference")
    print("after a 2px blur, which ignores single-pixel render noise.")
    px0, px1, py0, py1 = PEDAL_BOX
    in_sprite = np.zeros((height, width), bool)
    for _, (sx, sy, ex, ey) in sprites.values():
        in_sprite[sy:ey, sx:ex] = True
    checks = [
        ("mono, nothing on",     mono,   (False, False, False, 0)),
        ("stereo, nothing on",   stereo, (True,  False, False, 0)),
        ("mono, chorus I",       c1,     (False, True,  False, 1)),
        ("mono, chorus II",      c2,     (False, False, True,  2)),
        ("mono, chorus I + II",  both,   (False, True,  True,  3)),
    ]
    for label, real, combo in checks:
        rebuilt = composite(*combo)
        raw = np.abs(rebuilt.astype(np.int16) - real.astype(np.int16)).max(axis=2)
        blur = lambda im: ndimage.gaussian_filter(im.astype(np.float32), (2, 2, 0))
        soft = np.abs(blur(rebuilt) - blur(real)).max(axis=2)
        box = (slice(py0, py1), slice(px0, px1))
        inside = in_sprite[box]
        print("  %-20s controls: raw>8 %4d, visible max %5.1f | rest of pedal: raw>8 %4d, visible max %5.1f" % (
            label,
            (raw[box][inside] > 8).sum(), soft[box][inside].max(),
            (raw[box][~inside] > 8).sum(), soft[box][~inside].max()))
        if soft[box][inside].max() > 8:
            masked = np.where(inside, soft[box], 0)
            wy, wx = np.unravel_index(int(masked.argmax()), masked.shape)
            print("      worst control pixel at x=%d y=%d" % (wx + px0, wy + py0))

    # --- Preview image -------------------------------------------------------
    cx0, cy0, cx1, cy1 = 840, 320, 1100, 590
    union = np.maximum.reduce(list(alphas.values()))
    left = mono.astype(np.float32).copy()
    red = np.zeros_like(left)
    red[..., 0] = 255
    left = left * (1 - 0.55 * union[..., None]) + red * 0.55 * union[..., None]
    left = np.clip(left, 0, 255).astype(np.uint8)
    right = composite(True, True, True, 3)   # stereo, both on: never rendered

    k = 2
    panels = []
    for im, text in ((left, "alpha areas (red)"), (right, "stereo + I + II (never rendered)")):
        p = Image.fromarray(im[cy0:cy1, cx0:cx1]).resize(((cx1 - cx0) * k, (cy1 - cy0) * k), Image.LANCZOS)
        ImageDraw.Draw(p).text((8, 6), text, fill=(255, 255, 255))
        panels.append(p)
    sheet = Image.new("RGB", (panels[0].width * 2 + 12, panels[0].height), (20, 20, 20))
    sheet.paste(panels[0], (0, 0))
    sheet.paste(panels[1], (panels[0].width + 12, 0))
    sheet.save(args.preview)
    print()
    print("Preview written to", args.preview)


if __name__ == "__main__":
    main()
