# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

import asyncio
from machine import Timer
from time import sleep
from tc001 import FrameBuffer
from bcp import Event
from config import Config
# import network, ntptime

async def main():
    TIMER_INACTIVE = const(0)
    TIMER_REFRESHDATA = const(1)
    TIMER_COUNTDOWN = const(2)
    timer_state = [TIMER_INACTIVE] * 4
    timer = [Timer(i) for i in range(4)]

    DISPLAY_BOOTUP = const(0)
    DISPLAY_EVENTNAME = const(1)
    DISPLAY_ROUNDNUMBER = const(2)
    DISPLAY_COUNTDOWN = const(3)
    display_state = DISPLAY_BOOTUP

    config = Config()
    fb = FrameBuffer()

    # splash screen
    fb.clear()
    fb.text("BCP-clock", 0, 1)
    fb.show()

    # connect to WiFi here
    # network.hostname(config["hostname"])
    # sta_if = network.WLAN(network.WLAN.IF_STA)
    # sta_if.active(True)
    # sta_if.connect('ssid', 'password')
    # while not sta_if.isconnected():
    #     pass
    # ntptime.settime()

    # check if event is non empty, otherwise run config AP

    event = Event(config["event"])

    def display_round(round, total):
        """ Display current round indicator """
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
        """ Countdown timer callback — display remaining time """
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

    async def refresh_data():
        """ Async refresh data """
        try:
            await event.refresh()
        except Exception as e:
            print("Error refreshing data:", e)
            return
        nonlocal display_state
        if event.overview["status"]["ended"] or not event.overview["status"]["started"]:
            # event either not started or already ended; display event name
            if display_state != DISPLAY_EVENTNAME:
                if timer_state[1] == TIMER_COUNTDOWN:
                    timer[1].deinit()
                    timer_state[1] = TIMER_INACTIVE
                fb.clear()
                fb.text(event.overview["name"], 0, 1)
                fb.show()
                display_state = DISPLAY_EVENTNAME
        elif "timerLength" not in event.timer[event.overview['status']['currentRound']]:
            # no timer for current round; display round number only
            if display_state != DISPLAY_ROUNDNUMBER:
                if timer_state[1] == TIMER_COUNTDOWN:
                    timer[1].deinit()
                    timer_state[1] = TIMER_INACTIVE
                fb.clear()
                fb.text(f"[{event.overview['status']['currentRound']}]", 8, 1, font="f5x8")
                fb.show()
                display_state = DISPLAY_ROUNDNUMBER
        else:
            # run countdown timer
            print("Starting countdown timer")
            if timer_state[1] != TIMER_COUNTDOWN:
                if timer_state[1] != TIMER_INACTIVE:
                    timer[1].deinit()
                timer[1].init(period=config["countdowninterval"] * 1000, mode=Timer.PERIODIC, callback=countdown_callback)
                timer_state[1] = TIMER_COUNTDOWN
                display_state = DISPLAY_COUNTDOWN

    def refresh_callback(t):
        """ Event data refresh callback """
        try:
            loop = asyncio.get_event_loop()
            loop.create_task(refresh_data())
        except Exception as e:
            print("Cannot create refresh data task:", e)

    # we will get the event data refreshed periodically
    timer[0].init(period=config["refreshinterval"] * 1000, mode=Timer.PERIODIC, callback=refresh_callback)
    timer_state[0] = TIMER_REFRESHDATA
    refresh_callback(None)  # initial data fetch

    while True:
        await asyncio.sleep(1)
