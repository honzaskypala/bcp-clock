# Ulanzi TC001 driver
# (c) 2025 Honza Skýpala
# WTFPL license applies

# pins mapping:
#  matrix_pin: GPIO32 
#  buzzer_pin: GPIO15
#  battery_pin: GPIO34 
#  ldr_pin: GPIO35 
#  left_button_pin: GPIO26 
#  mid_button_pin: GPIO27 
#  right_button_pin: GPIO14 
#  scl_pin: GPIO22 
#  sda_pin: GPIO21 

from neopixel import NeoPixel
from machine import Pin
import struct

class BitmapFont:
    def __init__(self, fname):
        self._fontname = fname
    
    def __exit__(self, exc_type, exc_value, traceback):
        self._font.close()
    
    def __enter__(self):
        self._font = open(self._fontname, "rb")
        self.font_width, self.font_height = struct.unpack('BB', self._font.read(2))
        return self
    
    def textwidth(self, text):
        return len(text) * (self.font_width + 1) - 1

class FrameBuffer:
    DEFAULT_FONT = "f4x6"
    WIDTH = const(32)
    HEIGHT = const(8)
    _np = NeoPixel(Pin(32), 256)
    _fonts = {}
    
    def _get_font_path(self, **kwargs):
        fpath = "fonts/{}.bin".format(kwargs.get('font', FrameBuffer.DEFAULT_FONT))
        if fpath not in FrameBuffer._fonts:
            FrameBuffer._fonts[fpath] = BitmapFont(fpath)
        return fpath
    
    def fill(self, c):
        for i in range(256):
            FrameBuffer._np[i] = c
    
    def clear(self):
        self.fill((0, 0, 0))
    
    def pixel(self, x, y, c=None):
        if 0 <= x < FrameBuffer.WIDTH and 0 <= y < FrameBuffer.HEIGHT:
            i = (y + 1) * FrameBuffer.WIDTH - 1 - x if y % 2 else x + y * FrameBuffer.WIDTH
            if c is None:
                return FrameBuffer._np[i]
            else:
                FrameBuffer._np[i] = c
        else:
            raise ValueError(f"Invalid position ({x}, {y})")
            
    def hline(self, x, y, w, c):
        for i in range(w):
            self.pixel(x+i, y, c)
    
    def vline(self, x, y, h, c):
        for i in range(h):
            self.pixel(x, y+i, c)
            
    def line(self, x1, y1, x2, y2, c):
        dx = abs(x2 - x1)
        dy = -abs(y2 - y1)
        sx = 1 if x1 < x2 else -1
        sy = 1 if y1 < y2 else -1
        err = dx + dy

        while True:
            self.pixel(x1, y1, c)

            if x1 == x2 and y1 == y2:
                break

            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x1 += sx
            if e2 <= dx:
                err += dx
                y1 += sy
    
    def rect(self, x, y, w, h, c, f=False):
        if f:
            for py in range(y, y + h):
                for px in range(x, x + w):
                    self.pixel(px, py, c)
        else:
            for px in range(x, x + w):
                self.pixel(px, y, c)
                self.pixel(px, y + h - 1, c)
            for py in range(y, y + h):
                self.pixel(x, py, c)
                self.pixel(x + w - 1, py, c)
    
    def scroll(self, xstep, ystep):
        def scroll_ystep():
            if ystep >= 0:
                for y in reversed(range(ystep, FrameBuffer.HEIGHT)):
                    self.pixel(x, y, self.pixel(x - xstep, y - ystep))
            else:
                for y in range(0, FrameBuffer.HEIGHT + ystep):
                    self.pixel(x, y, self.pixel(x - xstep, y - ystep))
        if xstep >= 0:
            for x in reversed(range(xstep, FrameBuffer.WIDTH)):
                scroll_ystep()
        else:
            for x in range(0, FrameBuffer.WIDTH + xstep):
                scroll_ystep()
    
    def glyph(self, ch, x, y, **kwargs):
        fpath = self._get_font_path(**kwargs)
        c = kwargs.get('c', (255, 255, 255))
        with FrameBuffer._fonts[fpath] as f:
            xs = 0 if x >= 0 else abs(-x)
            xr = min(FrameBuffer.WIDTH - x, f.font_width)
            ys = 0 if y >= 0 else abs(-y)
            yr = min(FrameBuffer.HEIGHT - y, f.font_height)
            for char_x in range(xs, xr):
                f._font.seek(2 + (ord(ch) * f.font_width) + char_x)
                line = struct.unpack('B', f._font.read(1))[0]
                for char_y in range(ys, yr):
                    if (line >> char_y) & 0x1:
                        self.pixel(x + char_x, y + char_y, c)
            return f.font_width   # TODO: change to glyph width for proportional fonts
    
    def text(self, text, x, y, **kwargs):
        for i in range(len(text)):
            x += self.glyph(text[i], x, y, **kwargs) + 1
    
    def show(self):
        FrameBuffer._np.write()
