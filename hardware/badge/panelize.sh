#!/bin/bash
kikit panelize \
    --layout 'grid; rows: 2; cols: 2; space: 3mm; rotation: 180deg; alternation: rows;' \
    --tabs 'fixed; width: 3mm; hcount: 2; vcount: 1' \
    --cuts 'mousebites; drill: 0.5mm; spacing: 0.8mm; offset: 0.25mm' \
    --framing 'railslr; width: 5mm; space: 2mm;' \
    --tooling '3hole; hoffset: 2.5mm; voffset: 2.5mm; size: 1.152mm' \
    --fiducials '4fid; hoffset: 2.5mm; voffset: 5mm; coppersize: 2mm; opening: 1mm;' \
--post 'millradius: 0.5mm; reconstructarcs: true;' \
nsec-badge-2026.kicad_pcb panel/nsec-badge-2026-panel.kicad_pcb
