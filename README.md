# Steel Talons model viewer

Atari Lynx program (C++, llvm-mos) that shows every Steel Talons 3D model full
screen with its name. Based on a copy of `lynx_llvm_basecode_public`.

Controls: LEFT/RIGHT previous/next model, UP/DOWN tilt the camera, A toggles the spin.

## Build

```bash
python3 tools/build.py        # -> out/TalonsViewer.lnx
```

Run `python3 setup.py` once first: it fetches the submodules and builds the
toolchain.

The build runs `tools/gen_models.py`. It reads the models from the Steel
Talons repo, expected next to this one as `../lynx_steeltalons` (or set
`STEELTALONS`): `talons-build/POINTS/*.TSK` (through
`talons-build/tools/mkviewer.py`), the mission 1 palette and the game's font
(`finalsrc/DIGIT?.HCR`), and writes `src/game/models_data.h/.cpp`. Those two
files are committed, so without the Steel Talons repo the build uses them as
they are. Model names are the ones from `mkviewer.py`; the game itself
stores only the file names.

## Rendering

`gen_models.py` fan-triangulates the game's convex polygons and winds each
triangle by its polygon's normal. Each frame `src/game/main.cpp` rotates the
points and projects them to `u8` screen coordinates (the projection is sized
so the model always fits). It then draws the triangles in the model's own
order, skipping those whose screen winding shows their back. Like the game
(`DRAWPOLYGONS@` in DRAWOBJ.ASM) it does not sort: the models are built so
that order is right, details after what they sit on. A
triangle with a flat top or bottom is one Suzy sprite; any other triangle is
two, split at the middle vertex's row. A sprite is one literal pixel with
HPOS at the left edge, TILT following that edge, and STRETCH growing the width
by the right slope minus the left. Each sprite is drawn as soon as it is set
up, through one SCB. Flat models (bush, tree, runway…) are drawn double-sided.

Every multiply (rotation, projection, culling, slopes) runs on Suzy's math
unit in signed mode, through the inline starts in `sys/suzy_math.h`
(`StartSignedMultiply`, `AccumContext::Start`), which return at once so the
CPU prepares the next operands while one runs. Slopes multiply by a 16384/dy table instead of dividing.
Those functions are `NOINLINE` and use no C `*`, since the compiler's
`__mulhi3` also uses the math unit and could be scheduled in the middle.
