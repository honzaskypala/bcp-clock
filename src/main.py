# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

from machine import Timer, Pin
from time import sleep
from tc001 import FrameBuffer
from bcp import Event
from config import Config
# import network, ntptime

def main():
    TIMER_INACTIVE = const(0)
    TIMER_REFRESHDATA = const(1)
    TIMER_COUNTDOWN = const(2)
    timer_state = [TIMER_INACTIVE] * 4
    timer = [Timer(i) for i in range(4)]

    config = Config()
    fb = FrameBuffer()

    # connect to WiFi here
    # sta_if = network.WLAN(network.WLAN.IF_STA)
    # sta_if.active(True)
    # sta_if.connect('ssid', 'password')
    # while not sta_if.isconnected():
    #     pass
    # ntptime.settime()

    # check if event is non empty, otherwise run config AP

    event = Event(config["event"])

    def display_round(round, total):
        _active_color = (78, 159, 229)
        _inactive_color = (15, 15, 15)
        _line = const(7)
        fb.line(0, _line, 31, _line, (0, 0, 0))
        if total < FrameBuffer.WIDTH // 2:
            linewidth = FrameBuffer.WIDTH // total
            offset = {
                3: 2, 5: 2, 14: 2,
                7: 3, 9: 3, 13: 3,
                12: 4,
                11: 5
            }.get(total, 1)
            for r in range(total):
                fb.line(r * linewidth + offset, _line, (r + 1) * linewidth + offset - 2, _line, _active_color if r == round - 1 else _inactive_color)
        else:
            t = total if total <= FrameBuffer.WIDTH else FrameBuffer.WIDTH
            start = (FrameBuffer.WIDTH - (t + (2 if t <= FrameBuffer.WIDTH - 2 else 0))) // 2
            for r in range(t):
                if total <= FrameBuffer.WIDTH - 2 and r in (round - 1, round):
                    start += 1
                fb.pixel(start + r, _line, _active_color if r == round - 1 else _inactive_color)

    def countdown_callback(t):
        if event.overview["status"]["started"] and not event.overview["status"]["ended"]:
            remaining = event.remaining_seconds
            fb.clear()
            if remaining <= config["redseconds"]:
                c = (255, 0, 0)
            elif remaining <= config["yellowseconds"]:
                c = (255, 255, 0)
            else:
                c = (255, 255, 255)
            fb.text(event.remaining_time_str, 5 if remaining >= 0 else 0, 0, c=c)
            display_round(event.overview["status"]["currentRound"], event.overview["status"]["numberOfRounds"])
            fb.show()
        elif timer_state[1] == TIMER_COUNTDOWN:
            timer[1].deinit()
            timer_state[1] = TIMER_INACTIVE

    timer[0].init(period=config["refreshinterval"] * 1000, mode=Timer.PERIODIC, callback=lambda t: event.refresh())
    timer_state[0] = TIMER_REFRESHDATA
    
    if event.overview["status"]["started"] and not event.overview["status"]["ended"]:
        timer[1].init(period=config["countdowninterval"] * 1000, mode=Timer.PERIODIC, callback=countdown_callback)
        timer_state[1] = TIMER_COUNTDOWN
    else:
        fb.clear()
        fb.text(event.overview["name"], 0, 1, c=(255, 255, 255))
        fb.show()

    while True:
        sleep(1)

# using the WiFi or the display of the device heavily will disconnect the serial port.
# Therefore, you will not be able to access the device from your computer via serial.
# If you start the unit with the middle button pressed, the code will not run and
# the serial port will remain accessible.
if __name__ == "__main__" and Pin(27, Pin.IN, Pin.PULL_UP).value():
    main()
