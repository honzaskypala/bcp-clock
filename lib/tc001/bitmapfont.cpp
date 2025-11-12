#include "bitmapfont.h"
#include <FS.h>
#include <LittleFS.h>

BitmapFont::BitmapFont(String name) {
    this->filename = String("/fonts/") + name + ".bin";
    File r = LittleFS.open(this->filename, "r");
    if (!r) {
        Serial.println("Cannot open font");
    } else {
        this->_fileSize = r.size();
        this->_width = r.read();
        this->_height = r.read();
        r.close();
        // Serial.printf("Loaded font %s: %dx%d, %ld bytes\n", this->filename.c_str(), this->width, this->height, this->fileSize);
    }
}

void BitmapFont::getGlyph(char c, uint8_t* buffer) {
    File r = LittleFS.open(this->filename, "r");
    if (!r) {
        Serial.println("Cannot open font");
        return;
    }
    size_t offset = 2 + (static_cast<size_t>(c) * this->_width);
    if (offset + this->_width > this->_fileSize) {
        offset = 2; // Fallback to first glyph
    }
    r.seek(offset);
    r.read(buffer, this->_width);
    r.close();
}

int BitmapFont::textWidth(const String& str) {
    return str.length() * (this->_width + 1) - 1;
}