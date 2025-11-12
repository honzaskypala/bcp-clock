#include <Arduino.h>
#include <framebuffer.h>

void setup() {
  pinMode(15, INPUT_PULLDOWN);
}

void loop() {
  FrameBuffer.clear();
  FrameBuffer.hline(0, 0, CFrameBuffer::WIDTH, CRGB::Red, true);
  delay(1000);
  FrameBuffer.vline(5, 0, CFrameBuffer::HEIGHT, CRGB::Green, true);
  delay(1000);
  FrameBuffer.line(0, 0, CFrameBuffer::WIDTH - 1, CFrameBuffer::HEIGHT - 1, CRGB::Blue, true);
  delay(1000);
  FrameBuffer.rectangle(10, 2, 20, 6, CRGB::Yellow, true);
  delay(1000);
  FrameBuffer.filledRectangle(22, 1, 30, 5, CRGB::Purple, true);
  delay(1000);
  FrameBuffer.ellipse(16, 4, 6, 3, CRGB::Cyan, true);
  delay(1000);
  FrameBuffer.scroll(2, 0, CRGB::Black, true); 
  delay(1000);
  FrameBuffer.scroll(0, -2, CRGB::Black, true); 
  delay(1000);
}