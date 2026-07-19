#!/usr/bin/env python3
"""
Extract language-specific assets from a localized ROM.
Creates a language pack file that can be loaded on top of base assets.
"""

import sys
import os
import hashlib
import struct
import array

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
import util
import sprite_sheets
import text_compression

def pack_arrays(arr):
  if len(arr) == 0:
    return b''
  all_offs, offs = [], 0
  for i in range(len(arr) - 1):
    offs += len(arr[i])
    all_offs.append(offs)
  if offs < 65536 and len(arr) <= 8192:
    return b''.join([struct.pack('H', i) for i in all_offs] + arr + [struct.pack('H', len(arr) - 1)])
  else:
    return b''.join([struct.pack('I', i) for i in all_offs] + arr + [struct.pack('H', 8192 + len(arr) - 1)])

def load_rom(path, lang):
    rom = util.load_rom(path, support_multilanguage=True)
    if rom.language != lang:
        raise Exception(f"Expected language {lang}, got {rom.language}")
    return rom

def extract_link_graphics(rom):
    """Extract Link graphics (4bpp tileset)"""
    data = rom.get_bytes(0x108000, 0x800 * 448 // 32)  # Link graphics at 0x108000
    return data

def extract_dialogue(lang):
    """Extract compressed dialogue for language"""
    fname = text_compression.dialogue_filename(lang)
    # Check in assets/ directory first, then current directory
    if not os.path.exists(fname):
        fname = os.path.join('assets', fname)
    if not os.path.exists(fname):
        return None
    lines = []
    for line in open(fname, encoding='utf8').read().splitlines():
        a, b = line.split(': ', 1)
        lines.append(b)
    dict_packed = text_compression.encode_dictionary(lang)
    dialogue_packed = text_compression.compress_strings(lines, lang)
    return dict_packed, dialogue_packed

def extract_font_data(lang):
    """Encode font from PNG"""
    # Change to assets directory for font loading
    old_cwd = os.getcwd()
    os.chdir(os.path.dirname(__file__))
    try:
        font_data, font_width = sprite_sheets.encode_font_from_png(lang)
    finally:
        os.chdir(old_cwd)
    return font_data, font_width

def write_langpack(lang, assets, output_file):
    """Write language pack file"""
    key_sig = b''
    all_data = []
    all_names = []
    
    # Map asset names to indices
    name_to_idx = {
        'kLinkGraphics': 57,
        'kDialogue': 94,
        'kDialogueFont': 95,
        'kDialogueMap': 96,
    }
    
    # Build sorted list for consistent ordering
    for name in sorted(assets.keys()):
        data = assets[name]
        if isinstance(data, tuple):
            data = b''.join(data)
        key_sig += name.encode('utf8') + b'\0'
        all_data.append(data)
        all_names.append(name)
    
    # Map asset index to position in all_data (after all_names is populated)
    idx_to_pos = {}
    for i, name in enumerate(all_names):
        if name in name_to_idx:
            idx_to_pos[name_to_idx[name]] = i
    
    # Use same 48-byte signature format as base assets (16 bytes name + 32 bytes hash)
    assets_sig = b'Zelda3_LP_v0  \n\0' + hashlib.sha256(key_sig).digest()
    
    # Create full size array for all 165 assets
    all_sizes = array.array('I', [0] * 165)
    for i, name in enumerate(all_names):
        idx = name_to_idx.get(name, -1)
        if idx >= 0:
            all_sizes[idx] = len(all_data[i])
    
    # Header: 48-byte sig + 32 zero bytes + num_assets(4) + key_offset(4)
    hdr = assets_sig + b'\x00' * 32 + struct.pack('II', 165, len(key_sig))
    file_data = hdr + all_sizes + key_sig
    
    # Write asset data in index order (matching C code's read order)
    for i in range(165):
        if all_sizes[i] > 0 and i in idx_to_pos:
            data = all_data[idx_to_pos[i]]
            while len(file_data) & 3:
                file_data += b'\0'
            file_data += data
    
    with open(output_file, 'wb') as f:
        f.write(file_data)
    print(f"Wrote language pack: {output_file} ({len(file_data)} bytes)")

def main():
    if len(sys.argv) < 4:
        print("Usage: extract_langpack.py <lang> <rom_path> <output_file>")
        print("Example: extract_langpack.py pt zelda3_pt.sfc zelda3_langpack_pt.dat")
        sys.exit(1)
    
    lang = sys.argv[1]
    rom_path = sys.argv[2]
    output_file = sys.argv[3]
    
    if lang not in sprite_sheets.kFontTypes:
        print(f"Unknown language: {lang}")
        sys.exit(1)
    
    print(f"Loading {lang} ROM: {rom_path}")
    rom = load_rom(rom_path, lang)
    util.ROM = rom
    
    assets = {}
    
    # 1. Link graphics (4bpp)
    print("Extracting Link graphics...")
    link_gfx = extract_link_graphics(rom)
    assets['kLinkGraphics'] = link_gfx
    
    # 2. Font data
    print("Extracting font...")
    font_data, font_width = extract_font_data(lang)
    # kDialogueFont is a packed array of language entries
    # Each language entry is a packed array containing [font_data, font_width]
    font_entry = pack_arrays([font_data, font_width])
    assets['kDialogueFont'] = pack_arrays([font_entry])
    
    # 5. Dialogue
    print("Extracting dialogue...")
    dialogue = extract_dialogue(lang)
    if dialogue:
        dict_packed, dialogue_packed = dialogue
        # Pack dictionary entries and dialogue strings
        dict_data = pack_arrays(dict_packed)
        dialogue_data = pack_arrays(dialogue_packed)
        # kDialogue is a packed array of language entries
        # Each language entry is a packed array containing [dict_data, dialogue_data]
        lang_entry = pack_arrays([dict_data, dialogue_data])
        assets['kDialogue'] = pack_arrays([lang_entry])
    
    # 6. Language metadata - single entry for this language
    # Each language entry in kDialogueMap is a packed array of [name_bytes, config_bytes]
    flags = text_compression.uses_new_format(lang)
    name_bytes = lang.encode('utf8')  # no null terminator - strlen comparison
    config_bytes = struct.pack('BBB', 0, 0, flags)
    # kDialogueMap is a packed array containing [packed([name, config])]
    pt_entry = pack_arrays([name_bytes, config_bytes])
    assets['kDialogueMap'] = pack_arrays([pt_entry])
    
    write_langpack(lang, assets, output_file)

if __name__ == "__main__":
    main()