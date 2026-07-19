#!/usr/bin/env python3
"""Generate second_screen_tables.h from the icon/glyph tile data in render_icons.py.

This extracts the ITEMS and G dictionaries from render_icons.py and emits
the C header used by second_screen.c. Run from repo root:
  python3 tools/secondscreen/gen_tables.py
"""
import re, os

HERE = os.path.dirname(os.path.abspath(__file__))
RENDER_ICONS = os.path.join(HERE, "render_icons.py")
OUT = os.path.join(HERE, "..", "..", "src", "second_screen_tables.h")

src = open(RENDER_ICONS).read()

# exec just the table definitions (E, ITEMS, G) in a bare namespace
ns = {}
m = re.search(r"^E = .*?^}", src, re.S | re.M)
exec(m.group(0), ns)
mg = re.search(r"^G = \{\}.*?'mbar_empty':0x3cf5\}\)", src, re.S | re.M)
exec(mg.group(0), ns)

ITEMS, G = ns['ITEMS'], ns['G']

icon_entries = []
for name, lv in ITEMS.items():
    for i, vs in enumerate(lv):
        icon_entries.append(vs)
glyph_entries = list(G.values())

out = []
out.append("// GENERATED from tools/secondscreen/render_icons.py - do not edit by hand")
out.append(f"#define kIconCount {len(icon_entries)}")
out.append("#define kIconCols 10")
out.append("static const uint16 kIconTilemap[kIconCount][4] = {")
for vs in icon_entries:
    out.append("  {%s}," % ", ".join("0x%04x" % v for v in vs))
out.append("};")
out.append(f"#define kGlyphCount {len(glyph_entries)}")
out.append("#define kGlyphCols 12")
out.append("static const uint16 kGlyphTiles[kGlyphCount] = {")
out.append("  " + ", ".join("0x%04x" % v for v in glyph_entries))
out.append("};")

open(OUT, 'w').write("\n".join(out) + "\n")
print(f"wrote {OUT} — icons {len(icon_entries)}, glyphs {len(glyph_entries)}")
