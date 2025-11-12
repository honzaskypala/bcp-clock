#ifndef BITMAPFONT_H
#define BITMAPFONT_H

#include <Arduino.h>

class BitmapFont {
public:
    BitmapFont(String name);
    int width() const { return _width; }
    int height() const { return _height; }
    long fileSize() const { return _fileSize; }
    void getGlyph(char c, uint8_t* buffer);
    int textWidth(const String& str);

private:
    String filename;
    int _width, _height;
    long _fileSize;
};

#endif