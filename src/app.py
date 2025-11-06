# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

import asyncio
import machine
import time
from tc001 import FrameBuffer
from bcp import Event
import config
import network
import ntptime
import wifimgr
import socket
import gc

async def main():

    TIMER_INACTIVE = const(0)
    TIMER_REFRESH_DATA = const(1)
    TIMER_COUNTDOWN = const(2)
    TIMER_HTTP_CONFIG = const(3)

    timer_state = [TIMER_INACTIVE] * 4
    timer = [machine.Timer(i) for i in range(4)]

    # hardware timer assignment:
    # timer[0] : Refresh data @ 1 minute interval
    # timer[1] : Display countdown @ 1 sec interval
    # timer[2] : unused atm
    # timer[3] : config http server check update @ 1 sec interval and terminate after 5 minutes of inactivity

    DISPLAY_BOOTUP = const(0)
    DISPLAY_EVENTNAME = const(1)
    DISPLAY_ROUNDNUMBER = const(2)
    DISPLAY_COUNTDOWN = const(3)
    DISPLAY_SCROLL_ONCE = const(4)

    display_state = DISPLAY_BOOTUP

    async def setup_network():
        network.hostname(config.config["hostname"])
        wlan = wifimgr.get_connection(fb=fb, ssid="BCP-clock", device="BCP Clock")
        if wlan is None:
            # if no network, terminate
            fb.clear()
            fb.text("nonetwrk", 0, 2, font="f3x5")
            fb.show()
            import sys
            sys.exit()
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(('', 80))
            s.listen(5)
            s.close()
        except OSError as e:
            # if the code in try block above fails, then a web server from wifimgr.get_connection()
            # remains open, which is known bug of ESP32. Reboot the hardware to close it.
            machine.reset()
        try:
            ntptime.settime()
        except OSError as e:
            # if we cannot sync the time, terminate
            fb.clear()
            fb.text("timesync", 0, 2, font="f3x5")
            fb.show()
            await asyncio.sleep(5)
            fb.clear()
            fb.text("failure", 2, 2, font="f3x5")
            fb.show()
            import sys
            sys.exit()
        return wlan

    def display_round(round, total):
        """ Display current round indicator """
        active_color = (78, 159, 229)
        inactive_color = (15, 15, 15)
        line = const(7)
        fb.line(0, line, 31, line, (0, 0, 0))
        if total < FrameBuffer.WIDTH // 2:
            linewidth = FrameBuffer.WIDTH // total
            offset = {
                3: 2, 5: 2, 14: 2,
                7: 3, 9: 3, 13: 3,
                12: 4,
                11: 5
            }.get(total, 1)
            for r in range(total):
                fb.line(r * linewidth + offset, line, (r + 1) * linewidth + offset - 2, line, active_color if r == round - 1 else inactive_color)
        else:
            t = total if total <= FrameBuffer.WIDTH else FrameBuffer.WIDTH
            start = (FrameBuffer.WIDTH - (t + (2 if t <= FrameBuffer.WIDTH - 2 else 0))) // 2
            for r in range(t):
                if total <= FrameBuffer.WIDTH - 2 and r in (round - 1, round):
                    start += 1
                fb.pixel(start + r, line, active_color if r == round - 1 else inactive_color)

    def countdown_callback(t):
        """ Countdown timer callback — display remaining time """
        nonlocal display_state
        if display_state == DISPLAY_SCROLL_ONCE:
            return
        if event.overview["status"]["started"] and not event.overview["status"]["ended"]:
            remaining = event.remaining_seconds()
            fb.clear()
            if remaining <= config.config["red"]:
                c = (255, 0, 0)
            elif remaining <= config.config["yellow"]:
                c = (255, 255, 0)
            else:
                c = (255, 255, 255)
            if event.timer[event.overview['status']['currentRound']]['timerLength'] > 3600:
                font = "f3x5"
                x = 3
                x_minus = -1
                y = 1
                format = "{sign}{hours:01d}:{minutes:02d}:{seconds:02d}"
            else:
                font = "f4x6"
                x = 5
                x_minus = 0
                y = 0
                format = "{sign}{minutes:02d}:{seconds:02d}" if remaining > -3600 else "{sign}XX:XX"
            fb.text(event.remaining_time_str(format=format), x if remaining >= 0 else x_minus, y, c=c, font=font)
            display_round(event.overview["status"]["currentRound"], event.overview["status"]["numberOfRounds"])
            fb.show()
        elif timer_state[1] == TIMER_COUNTDOWN:
            timer[1].deinit()
            timer_state[1] = TIMER_INACTIVE

    async def refresh_data():
        """ Async refresh data """
        gc.collect()
        try:
            await event.refresh()
        except Exception as e:
            return
        nonlocal display_state
        if display_state == DISPLAY_SCROLL_ONCE:
            return
        elif event.overview["status"]["ended"] or not event.overview["status"]["started"]:
            # event either not started or already ended; display event name
            if display_state != DISPLAY_EVENTNAME:
                if timer_state[1] == TIMER_COUNTDOWN:
                    timer[1].deinit()
                    timer_state[1] = TIMER_INACTIVE
                fb.clear()
                fb.text(event.overview["name"], 0, 2, font="f3x5")
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
            if timer_state[1] != TIMER_COUNTDOWN:
                if timer_state[1] != TIMER_INACTIVE:
                    timer[1].deinit()
                timer[1].init(period=config.config["countdowninterval"] * 1000, mode=machine.Timer.PERIODIC, callback=countdown_callback)
                timer_state[1] = TIMER_COUNTDOWN
                display_state = DISPLAY_COUNTDOWN

    def refresh_callback(t):
        """ Event data refresh callback """
        try:
            loop = asyncio.get_event_loop()
            loop.create_task(refresh_data())
        except Exception as e:
            pass

    def mid_button_pressed(pin):

        def check_http_update(t):

            async def config_update():
                import cfgweb
                cfgweb.http_config_updated = False
                # nonlocal event
                # event = Event(config.config["event"])
                # refresh_callback(None)   # This does not work, request is sent, but we never get response. Therefore, rather reboot the device.
                await asyncio.sleep(1)     # Wait 1 sec, so the user gets http response before machine reboot
                machine.reset()

            nonlocal http_start_time
            import cfgweb
            if cfgweb.http_config_updated:
                try:
                    loop = asyncio.get_event_loop()
                    loop.create_task(config_update())
                except Exception as e:
                    pass
            elif time.time() - http_start_time > 5 * 60:
                # reset device after 5 minutes of config webserver open and no save
                machine.reset()

        async def config_splash():
            nonlocal display_state
            fb.clear()
            fb.text("Config", 0, 2, font="f3x5")
            fb.show()
            await asyncio.sleep(5)
            fb.clear()
            fb.text(".".join(wlan.ifconfig()[0].split(".")[:2]), 0, 2, font="f3x5")
            fb.show()
            await asyncio.sleep(5)
            fb.clear()
            fb.text(".".join(wlan.ifconfig()[0].split(".")[-2:]), 0, 2, font="f3x5")
            fb.show()
            await asyncio.sleep(5)
            display_state = DISPLAY_BOOTUP

        http_start_time = time.time()
        nonlocal display_state
        if display_state != DISPLAY_SCROLL_ONCE:
            display_state = DISPLAY_SCROLL_ONCE
            import cfgweb
            try:
                loop = asyncio.get_event_loop()
                loop.create_task(cfgweb.http_server())
                loop.create_task(config_splash())
            except Exception as e:
                pass
        
        timer[3].init(period=1000, mode=machine.Timer.PERIODIC, callback=check_http_update)
        timer_state[3] = TIMER_HTTP_CONFIG

    gc.enable()

    config.load_config()
    fb = FrameBuffer()

    # splash screen
    fb.clear()
    fb.text("BCPclock", 0, 1, font="f3x5")
    fb.show()

    wlan = await setup_network()

    if config.config["event"] == "":
        # event not configured yet, launch the config web
        mid_button_pressed(None)
        while True:
            await asyncio.sleep(1)

    event = Event(config.config["event"])

    # we will get the event data refreshed periodically
    timer[0].init(period=config.config["refreshinterval"] * 1000, mode=machine.Timer.PERIODIC, callback=refresh_callback)
    timer_state[0] = TIMER_REFRESH_DATA
    refresh_callback(None)  # initial data fetch

    button_pin = machine.Pin(27, machine.Pin.IN, machine.Pin.PULL_UP)
    button_pin.irq(trigger=machine.Pin.IRQ_FALLING, handler=mid_button_pressed)

    while True:
        await asyncio.sleep(1)