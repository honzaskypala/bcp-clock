// Bitmap font 4x6 pixels proportional width
// (c) 2025 Honza Skýpala
// Glyphs inspired by 4x6 font by fiftyclick, https://fontstruct.com/fontstructions/show/1650758/4x6-font
// WTFPL license applies

#pragma once

#include <Arduino.h>

const uint8_t F4x6Bitmaps[] PROGMEM = {
    0b11100000, 0b10011001, 0b01100110, 0b10011001, 0b01101001, 0b10011001, 0b10010110, 0b01011001, 
    0b00100101, 0b11000000, 0b01101001, 0b00010010, 0b01001111, 0b01101001, 0b00100001, 0b10010110, 
    0b10011001, 0b11110001, 0b00010001, 0b11111000, 0b11100001, 0b00011110, 0b01111000, 0b11101001, 
    0b10010110, 0b11110001, 0b00010001, 0b00010001, 0b01101001, 0b01101001, 0b10010110, 0b01101001, 
    0b01110001, 0b00010001, 0b10100000 
};

const GFXglyph F4x6Glyphs[] PROGMEM = {
    {     0, 3, 1, 4, 0, -3 },   // '-'
    {     0, 0, 0, 0, 0, 0 },    // '.'
    {     1, 4, 6, 5, 0, -6 },   // To save space in the font, 'X' bitmap mapped to '/' character
    {     4, 4, 6, 5, 0, -6 },   // '0'
    {     7, 3, 6, 5, 1, -6 },   // '1'
    {    10, 4, 6, 5, 0, -6 },   // '2'
    {    13, 4, 6, 5, 0, -6 },   // '3'
    {    16, 4, 6, 5, 0, -6 },   // '4'
    {    19, 4, 6, 5, 0, -6 },   // '5'
    {    22, 4, 6, 5, 0, -6 },   // '6'
    {    25, 4, 6, 5, 0, -6 },   // '7'
    {    28, 4, 6, 5, 0, -6 },   // '8'
    {    31, 4, 6, 5, 0, -6 },   // '9'
    {    34, 1, 3, 4, 1, -4 },   // ':'
};

const GFXfont F4x6 PROGMEM = {
    (uint8_t*)  F4x6Bitmaps,
    (GFXglyph*) F4x6Glyphs,
    45, 58, 7
};
