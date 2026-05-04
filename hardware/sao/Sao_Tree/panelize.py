#!/usr/bin/env python3
"""Stacked panel: tree (B1) on top, SAO badge (B2) below.

Auto-generates tabs on bounding-box edges. Any `kikit:Tab` footprints in the
source are ignored (cleared) so misconfigured ones don't break things.
"""

from pcbnewTransition import pcbnew
from pcbnewTransition.pcbnew import VECTOR2I, EDA_ANGLE, DEGREES_T
from shapely.geometry import box

from kikit.panelize import Panel, extractSourceAreaByAnnotation
from kikit.substrate import Substrate
from kikit.annotations import TabAnnotation
from kikit.units import mm

SRC = "Sao_Tree.kicad_pcb"
OUT = "Sao_Tree_panel.kicad_pcb"

HSPACE = 3 * mm
VSPACE = 3 * mm
FRAME_W = 5 * mm
TAB_WIDTH = 3 * mm
GAP_BETWEEN = 5 * mm

panel = Panel(OUT)

srcBoard = pcbnew.LoadBoard(SRC)
area_b1 = extractSourceAreaByAnnotation(srcBoard, "B1")
area_b2 = extractSourceAreaByAnnotation(srcBoard, "B2")

tree_w, tree_h = area_b1.GetWidth(), area_b1.GetHeight()
badge_w, badge_h = area_b2.GetWidth(), area_b2.GetHeight()

base_x, base_y = 30 * mm, 30 * mm
tree_cx = base_x + tree_w // 2

panel.appendBoard(
    filename=SRC,
    destination=VECTOR2I(tree_cx, base_y + tree_h // 2),
    sourceArea=area_b1,
    tolerance=1 * mm,
    rotationAngle=EDA_ANGLE(0, DEGREES_T),
)
panel.appendBoard(
    filename=SRC,
    destination=VECTOR2I(tree_cx, base_y + tree_h + GAP_BETWEEN + badge_h // 2),
    sourceArea=area_b2,
    tolerance=1 * mm,
    rotationAngle=EDA_ANGLE(0, DEGREES_T),
)


def ghostFrame(substrates, vSpace, hSpace, narrowX=None):
    """narrowX = (x0, x1) to narrow the top/bottom ghosts to that x-range,
    so the wide tree doesn't see ghost-bottom as a neighbor outside the
    badge's x-range (which would spawn extra tabs at concave spots)."""
    minx, miny, maxx, maxy = substrates[0].bounds()
    for s in substrates:
        mx, my, Mx, My = s.bounds()
        minx = min(minx, mx); miny = min(miny, my)
        maxx = max(maxx, Mx); maxy = max(maxy, My)
    w = 1 * mm
    tb_x0, tb_x1 = (narrowX if narrowX else (minx, maxx))
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


# Narrow top/bottom ghosts to the badge's x-range so the tree's bottom only
# sees the badge as a neighbor (no spurious ghost-bottom tabs on the sides).
badge_sub_bounds = panel.substrates[1].bounds()  # badge is 2nd substrate
ghosts = ghostFrame(panel.substrates, VSPACE, HSPACE,
                    narrowX=(badge_sub_bounds[0], badge_sub_bounds[2]))

panel.buildPartitionLineFromBB(ghosts)

panel.clearTabsAnnotations()
panel.buildTabAnnotationsFixed(
    hcount=0, vcount=1,
    hwidth=TAB_WIDTH, vwidth=TAB_WIDTH,
    minDistance=0,
    ghostSubstrates=ghosts,
)

# Add left/right side tabs on the tree at y-positions where the outline
# reaches the bbox edge (so the tab face stays short).
tree_sub = panel.substrates[0]
tree_minx, tree_miny, tree_maxx, tree_maxy = tree_sub.bounds()
tree_h_mm = tree_maxy - tree_miny
tree_w_mm = tree_maxx - tree_minx

# Nudge the auto-placed top tab slightly to the right
for a in tree_sub.annotations:
    if isinstance(a, TabAnnotation) and a.origin[1] < tree_miny + 1:
        a.origin = (a.origin[0] + tree_w_mm * 0.02, a.origin[1])
# Left edge of tree touches bbox at source y≈62 → 23% down from top
# Right edge of tree touches bbox at source y≈66 → 30% down from top
tree_sub.annotations.append(
    TabAnnotation(None, (tree_minx, tree_miny + tree_h_mm * 0.23),
                  (1, 0), TAB_WIDTH))
tree_sub.annotations.append(
    TabAnnotation(None, (tree_maxx, tree_miny + tree_h_mm * 0.32),
                  (-1, 0), TAB_WIDTH))
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
