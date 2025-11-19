// Bitmap font 3x5 pixels proportional width - General punctuation
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef P3X5_02010_H
#define P3X5_02010_H

#include <Arduino.h>

static const uint8_t P3X5_02010_BITMAPS[] PROGMEM = {
    2, 0b00000100, 0b00000100, 0b00000000, /* 0x2010 '-' hyphen */
    2, 0b00000100, 0b00000100, 0b00000000, /* 0x2011 '‑' non-breaking hyphen */
    3, 0b00000100, 0b00000100, 0b00000100, /* 0x2012 '—' figure dash */
    3, 0b00000100, 0b00000100, 0b00000100, /* 0x2013 '―' en dash */
    3, 0b00000100, 0b00000100, 0b00000100, /* 0x2014 '‒' em dash */
    3, 0b00000100, 0b00000100, 0b00000100, /* 0x2015 '―' horizontal bar */
    3, 0b00011111, 0b00000000, 0b00011111, /* 0x2016 '‖' double vertical line */
    3, 0b00010100, 0b00010100, 0b00010100, /* 0x2017 '‗' double low line */
    2, 0b00000011, 0b00000001, 0b00000000, /* 0x2018 '‘' left single quotation mark */
    2, 0b00000010, 0b00000011, 0b00000000, /* 0x2019 '’' right single quotation mark */
    2, 0b00010000, 0b00011000, 0b00000000, /* 0x201A '‚' single low-9 quotation mark */
    2, 0b00000011, 0b00000001, 0b00000000, /* 0x201B '‛' single high-reversed-9 quotation mark */
    3, 0b00000011, 0b00000000, 0b00000011, /* 0x201C '“' left double quotation mark */
    3, 0b00000011, 0b00000000, 0b00000011, /* 0x201D '”' right double quotation mark */
    3, 0b00011000, 0b00000000, 0b00011000, /* 0x201E '„' double low-9 quotation mark */
    3, 0b00000011, 0b00000000, 0b00000011, /* 0x201F '‟' double high-reversed-9 quotation mark */
    3, 0b00000010, 0b00011111, 0b00000010, /* 0x2020 '†' dagger */
    3, 0b00001010, 0b00011111, 0b00001010, /* 0x2021 '‡' double dagger */
    1, 0b00000100, 0b00000000, 0b00000000, /* 0x2022 '•' bullet */
    2, 0b00001110, 0b00000100, 0b00000000, /* 0x2023 '‣' triangular bullet */
    1, 0b00010000, 0b00000000, 0b00000000, /* 0x2024 '․' one dot leader */
    3, 0b00010000, 0b00000000, 0b00010000, /* 0x2025 '‥' two dot leader */
    3, 0b00010000, 0b00000000, 0b00010000, /* 0x2026 '…' horizontal ellipsis */
    1, 0b00000100, 0b00000000, 0b00000000, /* 0x2027 '‧' hyphenation point */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x2028 line separator */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x2029 paragraph separator */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202A left-to-right embedding */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202B right-to-left embedding */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202C pop directional formatting */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202D left-to-right override */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202E right-to-left override */
    1, 0b00000000, 0b00000000, 0b00000000, /* 0x202F narrow no-break space */
    3, 0b00001001, 0b00010100, 0b00010010, /* 0x2030 '‰' per mille sign */
    3, 0b00001001, 0b00010100, 0b00010010, /* 0x2031 '‱' per ten thousand sign */
    1, 0b00000110, 0b00000000, 0b00000000, /* 0x2032 '′' prime */
    3, 0b00000110, 0b00000000, 0b00000110, /* 0x2033 '″' double prime */
    3, 0b00000110, 0b00000000, 0b00000110, /* 0x2034 '‴' triple prime */
    1, 0b00000110, 0b00000000, 0b00000000, /* 0x2035 '‵' reversed prime */
    3, 0b00000110, 0b00000000, 0b00000110, /* 0x2036 '‶' double reversed prime */
    3, 0b00000110, 0b00000000, 0b00000110, /* 0x2037 '‷' triple reversed prime */
    3, 0b00010000, 0b00001000, 0b00010000, /* 0x2038 '‸' caret */
    2, 0b00000100, 0b00001010, 0b00000000, /* 0x2039 '‹' single left-pointing angle quotation mark */
    2, 0b00001010, 0b00000100, 0b00000000, /* 0x203A '›' single right-pointing angle quotation mark */
    3, 0b00001010, 0b00000100, 0b00001010, /* 0x203B '※' reference mark */
    3, 0b00010111, 0b00000000, 0b00010111, /* 0x203C '‼' double exclamation mark */
    3, 0b00000001, 0b00010111, 0b00000010, /* 0x203D '‽' interrobang */
    3, 0b00000001, 0b00000001, 0b00000001, /* 0x203E '‾' overline */
    3, 0b00001000, 0b00010000, 0b00001000, /* 0x203F '‿' undertie */
    3, 0b00000010, 0b00000001, 0b00000010, /* 0x2040 '⁀' character tie */
    3, 0b00010000, 0b00001000, 0b00010100, /* 0x2041 '⁁' caret insertion point */
    3, 0b00001000, 0b00000010, 0b00001000, /* 0x2042 '⁂' asterism */
    1, 0b00000100, 0b00000000, 0b00000000, /* 0x2043 '⁃' hyphen bullet */
    3, 0b00001000, 0b00000100, 0b00000010, /* 0x2044 '⁄' fraction slash */
    2, 0b00011111, 0b00010101, 0b00000000, /* 0x2045 '⁅' left square bracket with quill */
    2, 0b00010101, 0b00011111, 0b00000000, /* 0x2046 '⁆' right square bracket with quill */
    3, 0b00000001, 0b00010101, 0b00000010, /* 0x2047 '⁇' double question mark */
    3, 0b00000001, 0b00010101, 0b00010111, /* 0x2048 '⁈' question exclamation mark */
    3, 0b00010111, 0b00000001, 0b00010110, /* 0x2049 '⁉' exclamation question mark */
    3, 0b00000100, 0b00000100, 0b00011100, /* 0x204A '⁊' Tironian sign et */
    3, 0b00011111, 0b00000111, 0b00000010, /* 0x204B '⁋' reversed pilcrow sign */
    3, 0b00000100, 0b00001110, 0b00001110, /* 0x204C '⁌' black leftwards bullet */
    3, 0b00001110, 0b00001110, 0b00000100, /* 0x204D '⁍' black rightwards bullet */
    3, 0b00010100, 0b00001000, 0b00010100, /* 0x204E '⁎' low asterisk */
    1, 0b00011010, 0b00000000, 0b00000000, /* 0x204F '⁏' undertie */
    3, 0b00001010, 0b00010001, 0b00001010, /* 0x2050 '⁐' caret */
    3, 0b00010101, 0b00001010, 0b00010101, /* 0x2051 '⁑' two asterisks aligned vertically */
    3, 0b00001001, 0b00000100, 0b00010010, /* 0x2052 '⁒' commercial minus sign */
    3, 0b00000100, 0b00000100, 0b00000100, /* 0x2053 '⁓' swung dash */
    3, 0b00010000, 0b00001000, 0b00010000, /* 0x2054 '⁔' inverted swung dash */
    3, 0b00001010, 0b00000100, 0b00001010, /* 0x2055 '⁕' flower punctuation mark */
    3, 0b00000100, 0b00000000, 0b00001010, /* 0x2056 '⁖' three dot punctuation */
    3, 0b00000011, 0b00000000, 0b00000011, /* 0x2057 '⁗' quadruple prime */
    3, 0b00000100, 0b00001010, 0b00000100, /* 0x2058 '⁘' four dot punctuation */
    3, 0b00001010, 0b00000100, 0b00001010, /* 0x2059 '⁙' five dot punctuation */
    1, 0b00001010, 0b00000000, 0b00000000, /* 0x205A '⁚' two dot punctuation */
    3, 0b00000100, 0b00010001, 0b00000100, /* 0x205B '⁛' four dot punctuation */
    3, 0b00010101, 0b00001110, 0b00010101, /* 0x205C '⁜' dotted cross */
    1, 0b00010101, 0b00000000, 0b00000000, /* 0x205D '⁝' tricolon */
    1, 0b00010101, 0b00000000, 0b00000000, /* 0x205E '⁞' vertical four dot punctuation */
    2, 0b00000000, 0b00000000, 0b00000000  /* 0x205F medium mathematical space */
};

#endif