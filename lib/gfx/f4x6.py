# Convert 4x6 font to Adafruit GFX format
# (c) 2025 Honza Skýpala
# Glyphs inspired by 4x6 font by fiftyclick, https://fontstruct.com/fontstructions/show/1650758/4x6-font
# WTFPL license applies

WIDTH = 4
HEIGHT = 6
FIRST_CHAR = 45
OLDFONT = bytes((
    3, 0b00001000, 0b00001000, 0b00001000, 0b00000000, # -
    0, 0b00100000, 0b00000000, 0b00000000, 0b00000000, # .
    4, 0b00110011, 0b00001100, 0b00001100, 0b00110011, # To save space in the font, X bitmap mapped to / character
    4, 0b00011110, 0b00100001, 0b00100001, 0b00011110, # 0
    4, 0b00000000, 0b00100010, 0b00111111, 0b00100000, # 1
    4, 0b00100010, 0b00110001, 0b00101001, 0b00100110, # 2
    4, 0b00010010, 0b00100001, 0b00100101, 0b00011010, # 3
    4, 0b00000111, 0b00000100, 0b00000100, 0b00111111, # 4
    4, 0b00100111, 0b00100101, 0b00100101, 0b00011001, # 5
    4, 0b00011110, 0b00100101, 0b00100101, 0b00011001, # 6
    4, 0b00000001, 0b00000001, 0b00000001, 0b00111111, # 7
    4, 0b00011010, 0b00100101, 0b00100101, 0b00011010, # 8
    4, 0b00000010, 0b00000101, 0b00000101, 0b00111110, # 9
    3, 0b00000000, 0b00010100, 0b00000000, 0b00000000  # :
))

def construct_structure():
    offset = 0
    while offset < len(OLDFONT):
        width = OLDFONT[offset]
        bitmap = []
        for row in range(WIDTH):
            byte = OLDFONT[offset + 1 + row]
            bitmap.append(byte)
        yield (width, HEIGHT, bitmap)
        offset += 1 + WIDTH

def rotate_bitmap(struct):
    for n in range(len(struct)):
        width, height, bitmap = struct[n]
        new_bitmap = [0] * height
        for x in range(width):
            for y in range(height):
                if bitmap[x] & (1 << y):
                    new_bitmap[y] |= (1 << (7 - x))
        struct[n] = (width, height, new_bitmap)

def print_orig_bitmap(struct): 
    for n in range(len(struct)):
        width, height, bitmap = struct[n]
        print(f"Char {n + FIRST_CHAR} (width={width}):")
        for x in range(width):
            row = bitmap[x]
            line = ""
            for y in range(8):
                if row & (1 << (7 - y)):
                    line += "#"
                else:
                    line += "."
            print(line)
        print()

def print_bitmap(struct):
    for n in range(len(struct)):
        width, height, bitmap = struct[n]
        print(f"Char {n + FIRST_CHAR} (width={width}):")
        for y in range(height):
            row = bitmap[y]
            line = ""
            for x in range(8):
                if row & (1 << (7 - x)):
                    line += "#"
                else:
                    line += "."
            print(line)
        print()

struct = list(construct_structure())
#print("Original:")
#print_orig_bitmap(struct)
rotate_bitmap(struct)
#print("-------------------------")
#print("Rotated:")
#print_bitmap(struct)

arduino_bitmap = []
bitmap_offset = 0
arduino_glyphs = []

current_byte = 0
current_byte_bit = 128

for n in range(len(struct)):
    added = 0
    width, height, bitmap = struct[n]
    h = height
    yOffset = -height
    rangeLimit = height
    for y in range(height - 1, -1, -1):
        if bitmap[y] != 0:
            break
        rangeLimit -= 1
        h -= 1
    for y in range(0, rangeLimit):
        row = bitmap[y]
        if row == 0 and added == 0 and current_byte_bit == 128:
            h -= 1
            yOffset += 1
            continue
        for x in range(width):
            if row & (1 << (7 - x)):
                current_byte |= current_byte_bit
            current_byte_bit >>= 1
            if current_byte_bit == 0:
                arduino_bitmap.append(current_byte)
                added += 1
                current_byte = 0
                current_byte_bit = 128
    if current_byte_bit != 128:
        arduino_bitmap.append(current_byte)
        current_byte = 0
        current_byte_bit = 128
        added += 1
    arduino_glyphs.append((bitmap_offset, width, h, width + 1, 0, yOffset))
    bitmap_offset += added

print("#pragma once\n")
print("#include <Arduino.h>\n")
print("const uint8_t F4x6Bitmaps[] PROGMEM = {")
for i in range(len(arduino_bitmap)):
    if i % 8 == 0:
        print("    ", end="")
    print(f"0b{arduino_bitmap[i]:08b}, ", end="")
    if i % 8 == 7:
        print()
if len(arduino_bitmap) % 8 != 0:
    print()
print("};\n")
print("const GFXglyph F4x6Glyphs[] PROGMEM = {")
i = 0
for glyph in arduino_glyphs:
    print(f"    {{ {glyph[0]:5}, {glyph[1]}, {glyph[2]}, {glyph[3]}, {glyph[4]}, {glyph[5]} }},   // {chr(FIRST_CHAR + i)!r}")
    i += 1
print("};\n")
print("const GFXfont F4x6 PROGMEM = {")
print("    (uint8_t  *)F4x6Bitmaps,")
print("    (GFXglyph *)F4x6Glyphs,")
print(f"    {FIRST_CHAR}, {FIRST_CHAR + len(arduino_glyphs) - 1}, {HEIGHT+1}")
print("};")
