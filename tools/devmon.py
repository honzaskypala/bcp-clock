# Persistent serial monitor for the dev build (pio run -e dev)
# (c) 2025 Honza Skýpala
# WTFPL license applies
#
# Keeps the serial port open (opening/closing it resets the ESP32), appends all
# device output to a log file and forwards lines written to a FIFO as console
# commands. Use tools/devcmd.sh to send a command and read its response.
#
# usage: python tools/devmon.py [port] [log] [fifo]
#   defaults: first /dev/cu.usbserial-*, .pio/dev.log, .pio/dev.fifo

import glob
import os
import sys
import threading

import serial  # pyserial, available in ~/.platformio/penv/bin/python

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ports = glob.glob('/dev/cu.usbserial-*')
    port = sys.argv[1] if len(sys.argv) > 1 else (ports[0] if ports else None)
    log = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, '.pio', 'dev.log')
    fifo = sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOT, '.pio', 'dev.fifo')
    if port is None:
        sys.exit('No serial port found')
    if not os.path.exists(fifo):
        os.mkfifo(fifo)

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 9600
    ser.timeout = 0.2
    ser.dtr = False
    ser.rts = False
    ser.open()
    out = open(log, 'ab', buffering=0)

    def forward_commands():
        while True:
            with open(fifo, 'rb') as f:
                for line in f:
                    ser.write(line.rstrip(b'\r\n') + b'\n')

    threading.Thread(target=forward_commands, daemon=True).start()
    while True:
        data = ser.read(1024)
        if data:
            out.write(data)


if __name__ == '__main__':
    main()
