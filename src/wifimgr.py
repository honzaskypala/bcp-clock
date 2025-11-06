# Wi-Fi Manager
# (c) 2025 Honza Skýpala
# WTFPL license applies
# adapted from https://github.com/tayfunulu/WiFiManager by tayfunulu

import network
import time
import json

NETWORK_PROFILES = f'{__name__}.json'

def read_profiles():
    with open(NETWORK_PROFILES) as f:
        return json.load(f)

def write_profiles(profiles):
    with open(NETWORK_PROFILES, "w") as f:
        json.dump(profiles, f)

def do_connect(wlan_sta, ssid, password, fb=None):

    def display_progress():
        nonlocal fb_x, fb_s
        print('.', end='')
        if fb:
            fb.pixel(fb_x, 7, (0, 0, 0))
            if fb_x == 31 and fb_s == 1:
                fb_s = -1
            elif fb_x == 0 and fb_s == -1: 
                fb_s = 1
            fb_x += fb_s
            fb.pixel(fb_x, 7, (255, 255, 255))
            fb.show()

    wlan_sta.active(True)
    if wlan_sta.isconnected():
        return None
    print(f'Trying to connect to {ssid}...')
    wlan_sta.connect(ssid, password)
    fb_x = 0
    fb_s = 1
    for retry in range(200):
        connected = wlan_sta.isconnected()
        if connected:
            break
        time.sleep(0.1)
        display_progress()
    if connected:
        print('\nConnected. Network config: ', wlan_sta.ifconfig())
    else:
        print('\nFailed. Not Connected to: ' + ssid)
    if fb:
        fb.pixel(fb_x, 7, (0, 0, 0))
        fb.show()
    return connected

def get_connection(**kwargs):
    """return a working WLAN(STA_IF) instance or None"""
    wlan_sta = network.WLAN(network.STA_IF)

    fb = kwargs.get('fb', None)

    # First check if there already is any connection:
    if wlan_sta.isconnected():
        return wlan_sta

    connected = False

    try:
        # ESP connecting to WiFi takes time, wait a bit and try again:
        time.sleep(3)
        if wlan_sta.isconnected():
            return wlan_sta

        # Read known network profiles from file
        profiles = read_profiles()

        # Search WiFis in range
        wlan_sta.active(True)
        networks = wlan_sta.scan()

        AUTHMODE = {0: "open", 1: "WEP", 2: "WPA-PSK", 3: "WPA2-PSK", 4: "WPA/WPA2-PSK"}
        for ssid, bssid, channel, rssi, authmode, hidden in sorted(networks, key=lambda x: x[3], reverse=True):
            ssid = ssid.decode('utf-8')
            encrypted = authmode > 0
            print(f"ssid: {ssid} channel: {channel} rssi: {rssi} authmode: {AUTHMODE.get(authmode, '?')}")
            if encrypted:
                if ssid in profiles:
                    password = profiles[ssid]
                    connected = do_connect(wlan_sta, ssid, password, fb)
                else:
                    print("skipping unknown encrypted network")
            else:  # open
                connected = do_connect(wlan_sta, ssid, None, fb)
            if connected:
                break

    except OSError as e:
        print("exception", str(e))

    # start web server for connection manager:
    if not connected:
        import wificfg
        connected = wificfg.websrv_start(wlan_sta, kwargs)

    return wlan_sta if connected else None