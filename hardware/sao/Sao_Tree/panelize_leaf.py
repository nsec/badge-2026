#!/usr/bin/env python3
"""Leaf panel: B1, B2, B3 stacked vertically with tabs only at top/bottom."""

from pcbnewTransition import pcbnew
from pcbnewTransition.pcbnew import VECTOR2I, EDA_ANGLE, DEGREES_T
from shapely.geometry import box

from kikit.panelize import Panel, extractSourceAreaByAnnotation
from kikit.substrate import Substrate
from kikit.annotations import TabAnnotation
from kikit.units import mm

SRC = "Sao_Tree-leaf.kicad_pcb"
OUT = "Sao_Tree-leaf_panel.kicad_pcb"

HSPACE = 3 * mm
VSPACE = 3 * mm
FRAME_W = 5 * mm
TAB_WIDTH = int(2.5 * mm)
GAP = 5 * mm

panel = Panel(OUT)
srcBoard = pcbnew.LoadBoard(SRC)

areas = [extractSourceAreaByAnnotation(srcBoard, f"B{i}") for i in (1, 2, 3)]
widths = [a.GetWidth() for a in areas]
heights = [a.GetHeight() for a in areas]

base_x, base_y = 30 * mm, 30 * mm
cx = base_x + max(widths) // 2

y_cursor = base_y
for area, h in zip(areas, heights):
    panel.appendBoard(
        filename=SRC,
        destination=VECTOR2I(cx, y_cursor + h // 2),
        sourceArea=area,
        tolerance=1 * mm,
        rotationAngle=EDA_ANGLE(0, DEGREES_T),
    )
    y_cursor += h + GAP


def ghostFrame(substrates, vSpace, hSpace, narrowX):
    minx, miny, maxx, maxy = substrates[0].bounds()
    for s in substrates:
        mx, my, Mx, My = s.bounds()
        minx = min(minx, mx); miny = min(miny, my)
        maxx = max(maxx, Mx); maxy = max(maxy, My)
    w = 1 * mm
    tb_x0, tb_x1 = narrowX
    ghosts = []
    for poly in (
        box(tb_x0, miny - 2 * vSpace - w, tb_x1, miny - 2 * vSpace),
        box(tb_x0, maxy + 2 * vSpace, tb_x1, maxy + 2 * vSpace + w),
        box(minx - 2 * hSpace - w, miny, minx - 2 * hSpace, maxy),
        box(maxx + 2 * hSpace, miny, maxx + 2 * hSpace + w, maxy),
    ):
        s = Substrate([])
        s.union(poly)
        ghosts.append(s)
    return ghosts


# Narrow top/bottom ghosts to the narrowest board's x-range so cross-board
# tabs always land within the overlap (no concave-wrap chunks).
min_w = min(widths)
ghosts = ghostFrame(panel.substrates, VSPACE, HSPACE,
                    narrowX=(cx - min_w // 2, cx + min_w // 2))

panel.buildPartitionLineFromBB(ghosts)

panel.clearTabsAnnotations()
panel.buildTabAnnotationsFixed(
    hcount=0, vcount=1,
    hwidth=TAB_WIDTH, vwidth=TAB_WIDTH,
    minDistance=0,
    ghostSubstrates=ghosts,
)
# Tab shifts. Frame-facing tabs (B1-top, B3-bottom) stay centered.
# B1↔B2 internal tab: 0.1 × leaf width to the left.
# B2↔B3 internal tab: 0.1 × leaf width left, then nudged 2mm right.
shift_b1b2 = int(widths[1] * 0.1)
shift_b2b3 = int(widths[1] * 0.1) - int(4.2 * mm)
for i, sub in enumerate(panel.substrates):
    for a in sub.annotations:
        if not isinstance(a, TabAnnotation):
            continue
        at_top = a.direction[1] > 0.5
        at_bottom = a.direction[1] < -0.5
        if i == 0 and at_bottom:            # B1 bottom → B1B2 tab
            a.origin = (a.origin[0] - shift_b1b2, a.origin[1])
        elif i == 1 and at_top:              # B2 top → B1B2 tab
            a.origin = (a.origin[0] - shift_b1b2, a.origin[1])
        elif i == 1 and at_bottom:           # B2 bottom → B2B3 tab
            a.origin = (a.origin[0] - shift_b2b3, a.origin[1])
        elif i == 2 and at_top:              # B3 top → B2B3 tab
            a.origin = (a.origin[0] - shift_b2b3, a.origin[1])
        elif i == 2 and at_bottom:           # B3 bottom → frame, nudge right
            a.origin = (a.origin[0] + int(1.5 * mm), a.origin[1])

tabCuts = panel.buildTabsFromAnnotations(fillet=0)

frameCutsV, frameCutsH = panel.makeFrame(
    widthH=FRAME_W, widthV=FRAME_W,
    hspace=HSPACE, vspace=VSPACE,
)

panel.makeMouseBites(
    cuts=list(frameCutsV) + list(frameCutsH) + list(tabCuts),
    diameter=int(0.5 * mm),
    spacing=int(0.8 * mm),
    offset=int(-0.25 * mm),
)

panel.addMillFillets(millRadius=1 * mm)
panel.save()
print(f"wrote {OUT}")
