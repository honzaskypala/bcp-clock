# datetime.strptime missing in MicroPython
# (c) 2025 Honza Skýpala
# WTFPL license applies

import time

def strptime(timestamp):
    timestamp = timestamp.rstrip("Z")
    date_str, time_str = timestamp.split("T")
    year, month, day = map(int, date_str.split("-"))
    hour, minute, second = map(int, time_str.split(":"))
    return time.mktime((year, month, day, hour, minute, second, 0, 0))
