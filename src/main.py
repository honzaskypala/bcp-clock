# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

import asyncio
from machine import Pin
from app import main

# using the WiFi or the display of the device heavily will disconnect the serial port.
# Therefore, you will not be able to access the device from your computer via serial.
# If you start the unit with the middle button pressed, the code will not run and
# the serial port will remain accessible.
if __name__ == "__main__" and Pin(27, Pin.IN, Pin.PULL_UP).value():
    asyncio.run(main())
