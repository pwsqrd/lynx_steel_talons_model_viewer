#!/usr/bin/env python3
"""
Generates src/game/models_data.h/.cpp from the Steel Talons sources.

    python3 tools/gen_models.py

The Steel Talons repo is expected next to this one (../lynx_steeltalons), or
wherever STEELTALONS points. Without it the committed models_data files are
kept as they are.

Inputs (paths relative to the lynx_steeltalons repo):
    talons-build/POINTS/*.TSK     models, read with talons-build/tools/mkviewer.py
    talons-build/fastsrc/M1PAL.RAW  mission 1 palette (palette type 0)
    finalsrc/DIGIT?.HCR           the game's 2bpp font (high score screen)

Per model the points are centred on their bounding box and scaled so the
furthest point is RADIUS units out, then stored as s8. The game's polygons
are convex, so each is stored as a fan of triangles sharing its colour,
each wound so (b - a) x (c - a) points along the polygon's normal; 2-point polygons
stay lines. The normals themselves are not stored.
Polygon colours go through the game's COLORMAPPING and
COLORSHADING (XFORM.ASM) here, because the sun is fixed in model space and a
face's shade never changes while the viewer only turns the camera.
"""
import math
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = Path(os.environ.get('STEELTALONS', ROOT.parent / 'lynx_steeltalons'))
TALONS = REPO / 'talons-build'
if not TALONS.is_dir():
    print(f'gen_models.py: {REPO} not found, keeping the committed models_data files')
    sys.exit(0)
sys.path.insert(0, str(TALONS / 'tools'))
import mkviewer  # noqa: E402

OUT_H = ROOT / 'src' / 'game' / 'models_data.h'
OUT_CPP = ROOT / 'src' / 'game' / 'models_data.cpp'

RADIUS = 120            # model radius in view units (s8)
CAM_DIST = 320          # camera distance from the model centre, view units
FOCAL = 120             # screen pixels per unit at depth CAM_DIST / FOCAL
# The radius sphere seen from CAM_DIST spans FOCAL * RADIUS / sqrt(CAM_DIST^2
# - RADIUS^2) = 48.6 pixels from the centre, so every point is on screen.

MISSION = 1
GROUPS = ['Aircraft', 'Ground targets', 'Structures', 'Weapons', 'Scenery', 'Terrain', 'Misc']

# DRAWOBJ.ASM / XFORM.ASM tables, as in talons-build/tools/viewer.html
MISSIONPALTYPES = [1, 0, 1, 2, 1, 2, 1, 0, 2, 0, 1, 0, 2]
COLORMAPPING = [0, 1, 2, 3, 4, 7, 9, 10, 15, 5, 6, 8, 7, 10, 10, 10,
                0, 1, 2, 3, 4, 7, 9, 5, 5, 15, 6, 8, 7, 10, 10, 10,
                0, 1, 2, 3, 4, 7, 9, 5, 6, 12, 15, 8, 7, 10, 10, 13]
COLORSHADING = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 6, 6, 6,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 6, 6, 6,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3]
SHADED3 = [0, 0, 0, 0, 0, 1, 1, 2, 2, 4, 4, 4, 4, 4, 4, 4,
           4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0]
SHADED6 = [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5,
           5, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
           7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
SUN_YAW = 0x080 / 1024 * 2 * math.pi        # SUNYROT
SUN = (math.sin(math.pi / 4) * math.cos(SUN_YAW),
       math.sin(math.pi / 4) * math.sin(SUN_YAW),
       math.cos(math.pi / 4))

FONT_CHARS = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'


def face_color(c, normal):
    x = MISSIONPALTYPES[MISSION] * 16 + (c & 15)
    col = COLORMAPPING[x]
    n = COLORSHADING[x]
    if n:
        length = math.sqrt(sum(v * v for v in normal)) or 1
        dot = sum(v * s for v, s in zip(normal, SUN)) / length
        s = max(-64, min(63, round(dot * 64))) & 255
        col += SHADED3[s >> 3] if n == 3 else SHADED6[s >> 2]
    return col & 15


def font_glyph(ch):
    text = (REPO / 'finalsrc' / f'DIGIT{ch}.HCR').read_text()
    data = []
    for line in text.splitlines():
        line = line.split(';')[0].strip()
        if line.upper().startswith('DB'):
            data += [int(v.strip().lstrip('$'), 16) for v in line[2:].split(',')]
    return data


def s8(v):
    return max(-127, min(127, round(v)))


def byte_rows(values, per_row=16):
    values = list(values)
    return ',\n'.join('    ' + ', '.join(str(v) for v in values[i:i + per_row])
                      for i in range(0, len(values), per_row))


def load_models():
    models = []
    for path in sorted((TALONS / 'POINTS').glob('*.TSK')):
        m = mkviewer.readtsk(path)
        m['file'] = path.stem
        m['name'], m['group'] = mkviewer.label(path.stem)
        models.append(m)
    models.sort(key=lambda m: GROUPS.index(m['group']))
    return models


def convert(m):
    pts = m['pts']
    lo = [min(p[k] for p in pts) for k in range(3)]
    hi = [max(p[k] for p in pts) for k in range(3)]
    centre = [(a + b) / 2 for a, b in zip(lo, hi)]
    reach = max(math.dist(p, centre) for p in pts) or 1
    scale = RADIUS / reach
    points = [s8((p[k] - centre[k]) * scale) for p in pts for k in range(3)]
    faces = []
    normal_ids = set()
    for _, ni, col, idx in m['polys']:
        normal = m['nrm'][ni] if ni < len(m['nrm']) else (0, 0, 0x4000)
        normal_ids.add(ni)
        head = [face_color(col, normal)]
        if len(idx) < 3:
            faces.append(head + [2] + idx)
        for k in range(1, len(idx) - 1):
            tri = [idx[0], idx[k], idx[k + 1]]
            # Wind every triangle so (b - a) x (c - a) points along the
            # normal; the viewer culls by its sign on screen.
            if winding(points, tri, normal) < 0:
                tri[1], tri[2] = tri[2], tri[1]
            faces.append(head + [3] + tri)
    name = ''.join(c if c in FONT_CHARS else ' ' for c in m['name'].upper())
    return points, faces, name, len(normal_ids) == 1


def winding(points, tri, normal):
    p0, p1, p2 = (points[i * 3:i * 3 + 3] for i in tri)
    e1 = [b - a for a, b in zip(p0, p1)]
    e2 = [b - a for a, b in zip(p0, p2)]
    cross = (e1[1] * e2[2] - e1[2] * e2[1],
             e1[2] * e2[0] - e1[0] * e2[2],
             e1[0] * e2[1] - e1[1] * e2[0])
    return sum(c * n for c, n in zip(cross, normal))


def main():
    models = load_models()
    raw = (TALONS / 'fastsrc' / f'M{MISSION:X}PAL.RAW').read_bytes()

    def luma(i):
        return 2 * (raw[i] & 15) + (raw[16 + i] & 15) + (raw[16 + i] >> 4)
    text_pen = max(range(1, 16), key=luma)
    shadow_pen = min(range(1, 16), key=luma)

    glyphs = [font_glyph(c) for c in FONT_CHARS]
    offsets, font = [], []
    for g in glyphs:
        offsets.append(len(font))
        font += g

    converted = [convert(m) for m in models]
    max_points = max(len(m['pts']) for m in models)
    max_faces = max(len(c[1]) for c in converted)

    h = f'''// Generated by tools/gen_models.py from the Steel Talons sources. Do not edit.
#ifndef MODELS_DATA_H
#define MODELS_DATA_H

#include "sys/types.h"

// Points are centred and scaled to radius {RADIUS}. Each face is: palette
// index, vertex count (3, or 2 for a line), point indices. A triangle's
// (b - a) x (c - a) points along its polygon's normal.
struct Model
{{
    const char* name;
    const s8*   points;
    const u8*   faces;
    u8          numPoints;
    u8          numFaces;
    bool        twoSided;   // flat model (bush, tree, runway...)
}};

constexpr u8  MODEL_COUNT      = {len(models)};
constexpr u8  MODEL_MAX_POINTS = {max_points};
constexpr u8  MODEL_MAX_FACES  = {max_faces};
constexpr s16 MODEL_CAM_DIST   = {CAM_DIST};
constexpr u8  BACKGROUND_PEN   = 0;
constexpr u8  TEXT_PEN         = {text_pen};
constexpr u8  TEXT_SHADOW_PEN  = {shadow_pen};
constexpr u8  FONT_ADVANCE     = 5;
constexpr u8  FONT_HEIGHT      = 7;
constexpr u8  SLOPE_RECIP_SIZE = 128;

extern const Model g_models[MODEL_COUNT];
extern const u8    g_palette[32];

// sin(i * 2pi / 256) * 64
extern const s8    g_sin[256];

// FOCAL * 256 / (depth + CAM_DIST), indexed by depth + 128
extern const u8    g_recip[256];

// 16384 / dy for slopes (0 for dy = 0)
extern const u16   g_slopeRecip[SLOPE_RECIP_SIZE];

// The game's 2bpp packed font: '0'..'9' then 'A'..'Z'.
extern const u8    g_font[];
extern const u16   g_fontOffset[36];

#endif
'''

    cpp = ['// Generated by tools/gen_models.py from the Steel Talons sources. Do not edit.',
           '#include "models_data.h"', '']
    table = []
    for i, (m, (points, faces, name, two_sided)) in enumerate(zip(models, converted)):
        cpp.append(f'// {m["file"]}.TSK: {m["name"]} ({m["group"]})')
        cpp.append(f'static const s8 s_points{i}[] = {{\n{byte_rows(points, 15)}\n}};')
        cpp.append(f'static const u8 s_faces{i}[] = {{\n{byte_rows(sum(faces, []))}\n}};')
        cpp.append('')
        table.append(f'    {{ "{name}", s_points{i}, s_faces{i}, '
                     f'{len(m["pts"])}, {len(faces)}, {"true" if two_sided else "false"} }},')
    cpp.append('const Model g_models[MODEL_COUNT] = {')
    cpp += table
    cpp.append('};')
    cpp.append('')
    cpp.append(f'const u8 g_palette[32] = {{\n{byte_rows(raw)}\n}};')
    cpp.append('')
    sin = [s8(math.sin(i * 2 * math.pi / 256) * 64) for i in range(256)]
    cpp.append(f'const s8 g_sin[256] = {{\n{byte_rows(sin)}\n}};')
    cpp.append('')
    recip = [min(255, round(FOCAL * 256 / (i - 128 + CAM_DIST))) for i in range(256)]
    cpp.append(f'const u8 g_recip[256] = {{\n{byte_rows(recip)}\n}};')
    cpp.append('')
    slope = [0] + [round(16384 / dy) for dy in range(1, 128)]
    cpp.append(f'const u16 g_slopeRecip[SLOPE_RECIP_SIZE] = {{\n{byte_rows(slope, 12)}\n}};')
    cpp.append('')
    cpp.append(f'const u8 g_font[] = {{\n{byte_rows(font)}\n}};')
    cpp.append(f'const u16 g_fontOffset[36] = {{\n{byte_rows(offsets, 12)}\n}};')

    OUT_H.write_text(h)
    OUT_CPP.write_text('\n'.join(cpp) + '\n')
    print(f'{OUT_CPP.name}: {len(models)} models, font {len(font)} bytes, '
          f'text pen {text_pen}, shadow pen {shadow_pen}')


if __name__ == '__main__':
    main()
