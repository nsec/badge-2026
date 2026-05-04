#!/usr/bin/env python3
"""Minimal panel: just boards + frame, no tabs, no fillets — to isolate geometry."""

from pcbnewTransition import pcbnew
from pcbnewTransition.pcbnew import VECTOR2I, EDA_ANGLE, DEGREES_T
from kikit.panelize import Panel, extractSourceAreaByAnnotation
from kikit.substrate import Substrate
from kikit.units import mm
from shapely.geometry import box

SRC = "Sao_Tree.kicad_pcb"
OUT = "Sao_Tree_panel_debug.kicad_pcb"

panel = Panel(OUT)
srcBoard = pcbnew.LoadBoard(SRC)
area_b1 = extractSourceAreaByAnnotation(srcBoard, "B1")
area_b2 = extractSourceAreaByAnnotation(srcBoard, "B2")

tree_w, tree_h = area_b1.GetWidth(), area_b1.GetHeight()
badge_w, badge_h = area_b2.GetWidth(), area_b2.GetHeight()

panel.appendBoard(
    filename=SRC,
    destination=VECTOR2I(30*mm + tree_w // 2, 30*mm + tree_h // 2),
    sourceArea=area_b1, tolerance=1*mm,
    rotationAngle=EDA_ANGLE(0, DEGREES_T),
    interpretAnnotations=False,
)
panel.appendBoard(
    filename=SRC,
    destination=VECTOR2I(30*mm + tree_w // 2,
                         30*mm + tree_h + 5*mm + badge_h // 2),
    sourceArea=area_b2, tolerance=1*mm,
    rotationAngle=EDA_ANGLE(0, DEGREES_T),
    interpretAnnotations=False,
)

# Ghost frame substrates
minx, miny, maxx, maxy = panel.substrates[0].bounds()
for s in panel.substrates:
    mx, my, Mx, My = s.bounds()
    minx = min(minx, mx); miny = min(miny, my)
    maxx = max(maxx, Mx); maxy = max(maxy, My)
w = 1 * mm
ghosts = []
for poly in (
    box(minx, miny - 2*3*mm - w, maxx, miny - 2*3*mm),
    box(minx, maxy + 2*3*mm, maxx, maxy + 2*3*mm + w),
    box(minx - 2*3*mm - w, miny, minx - 2*3*mm, maxy),
    box(maxx + 2*3*mm, miny, maxx + 2*3*mm + w, maxy),
):
    s = Substrate([]); s.union(poly); ghosts.append(s)

panel.buildPartitionLineFromBB()  # NO ghosts
panel.clearTabsAnnotations()
panel.buildTabAnnotationsFixed(
    hcount=0, vcount=1,
    hwidth=3*mm, vwidth=3*mm,
    minDistance=0, ghostSubstrates=ghosts)

# List all annotations after generation
for i, s in enumerate(panel.substrates):
    print(f'substrate {i}: {len(s.annotations)} annotations')
    for a in s.annotations:
        print(f'  {type(a).__name__}: origin={a.origin}, dir={a.direction}, width={a.width}')
tabCuts = panel.buildTabsFromAnnotations(fillet=0)
panel.makeFrame(widthH=5*mm, widthV=5*mm, hspace=3*mm, vspace=3*mm)
panel.save()
print(f"wrote {OUT}")
