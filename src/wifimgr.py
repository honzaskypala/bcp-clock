# Wi-Fi Manager
# (c) 2025 Honza Skýpala
# WTFPL license applies
# adapted from https://github.com/tayfunulu/WiFiManager by tayfunulu

import network
import socket
import re
import time
import json

def get_connection(**kwargs):
    """return a working WLAN(STA_IF) instance or None"""
    ap_ssid = kwargs.get('ssid', "ESP-32")
    ap_password = kwargs.get('password', "")
    ap_authmode = 3  # WPA2

    NETWORK_PROFILES = 'wifi.json'

    wlan_ap = network.WLAN(network.AP_IF)
    wlan_sta = network.WLAN(network.STA_IF)

    server_socket = None

    fb = kwargs.get('fb', None)
    fb_x = 0
    fb_s = 1

    devicename = kwargs.get('device', 'ESP-32')

    # First check if there already is any connection:
    if wlan_sta.isconnected():
        return wlan_sta

    connected = False

    def read_profiles():
        with open(NETWORK_PROFILES) as f:
            return json.load(f)

    def write_profiles(profiles):
        with open(NETWORK_PROFILES, "w") as f:
            json.dump(profiles, f)

    def do_connect(ssid, password):
        wlan_sta.active(True)
        if wlan_sta.isconnected():
            return None
        print('Trying to connect to %s...' % ssid)
        wlan_sta.connect(ssid, password)
        nonlocal fb_x, fb_s
        for retry in range(200):
            connected = wlan_sta.isconnected()
            if connected:
                break
            time.sleep(0.1)
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
        if connected:
            print('\nConnected. Network config: ', wlan_sta.ifconfig())
            
        else:
            print('\nFailed. Not Connected to: ' + ssid)
        if fb:
            fb.pixel(fb_x, 7, (0, 0, 0))
            fb.show()

        return connected

    def send_header(client, status_code=200, content_length=None ):
        client.sendall("HTTP/1.0 {} OK\r\n".format(status_code))
        client.sendall("Content-Type: text/html\r\n")
        if content_length is not None:
            client.sendall("Content-Length: {}\r\n".format(content_length))
        client.sendall("\r\n")

    def send_response(client, payload, status_code=200):
        content_length = len(payload)
        send_header(client, status_code, content_length)
        if content_length > 0:
            client.sendall(payload)
        client.close()

    def get_css():
        try:
            with open('wifi.css', 'r') as file:
                return file.read()
        except FileNotFoundError:
            return ''

    def handle_root(client):
        css = get_css()
        wlan_sta.active(True)
        ssids = sorted(ssid.decode('utf-8') for ssid, *_ in wlan_sta.scan())
        send_header(client)
        nonlocal devicename
        client.sendall(f'<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1>{devicename} Wi-Fi Setup</h1><form action="configure" method="post">')
        if len(ssids):
            while len(ssids):
                ssid = ssids.pop(0)
                if ssid != "":
                    client.sendall(f'<span class="ssid"><input type="radio" name="ssid" value="{ssid}" id="{ssid}"/><label for="{ssid}">{ssid}</label></span>')
            client.sendall('<label for="password">Password:</label><input name="password" type="password" id="password" /><input type="submit" value="Submit" /></form></html>')
        else:
            client.sendall('No networks found.</html>')
        client.close()

    def handle_configure(client, request):
        match = re.search("ssid=([^&]*)&password=(.*)", request)

        if match is None:
            send_response(client, "Parameters not found", status_code=400)
            return False
        # version 1.9 compatibility
        try:
            ssid = match.group(1).decode("utf-8").replace("%3F", "?").replace("%21", "!")
            password = match.group(2).decode("utf-8").replace("%3F", "?").replace("%21", "!")
        except Exception:
            ssid = match.group(1).replace("%3F", "?").replace("%21", "!")
            password = match.group(2).replace("%3F", "?").replace("%21", "!")

        if len(ssid) == 0:
            send_response(client, "SSID must be provided", status_code=400)
            return False

        nonlocal devicename
        css = get_css()
        if do_connect(ssid, password):
            response = f'<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1 class="success">{devicename} successfully connected to WiFi network {ssid}</h1></html>'
            send_response(client, response)
            time.sleep(1)
            wlan_ap.active(False)
            try:
                profiles = read_profiles()
            except OSError:
                profiles = {}
            profiles[ssid] = password
            write_profiles(profiles)

            time.sleep(5)

            return True
        else:
            response = f'<html><head class="error"><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1 class="error">{devicename} could not connect to WiFi network {ssid}</h1><form><input type="button" value="Go back!" onclick="history.back()"></input></form></html>'
            send_response(client, response)
            return False

    def handle_not_found(client, url):
        send_response(client, "Path not found: {}".format(url), status_code=404)

    def websrv_stop():
        nonlocal server_socket

        if server_socket:
            server_socket.close()
            server_socket = None

    def websrv_start(port=80):
        nonlocal server_socket

        addr = socket.getaddrinfo('0.0.0.0', port)[0][-1]

        websrv_stop()

        wlan_sta.active(True)
        wlan_ap.active(True)

        wlan_ap.config(essid=ap_ssid, password=ap_password)

        server_socket = socket.socket()
        server_socket.bind(addr)
        server_socket.listen(1)

        print('Connect to WiFi ssid ' + ap_ssid + ', default password: ' + ap_password)
        print('and access the ESP via your favorite web browser at 192.168.4.1.')
        print('Listening on:', addr)

        if fb:
            fb.clear()
            fb.text("AP mode", 0, 1)
            fb.show()

        while True:
            if wlan_sta.isconnected():
                wlan_ap.active(False)
                return True

            client, addr = server_socket.accept()
            print('client connected from', addr)
            try:
                client.settimeout(5.0)

                request = b""
                try:
                    while "\r\n\r\n" not in request:
                        request += client.recv(512)
                except OSError:
                    pass

                # Handle form data from Safari on macOS and iOS; it sends \r\n\r\nssid=<ssid>&password=<password>
                try:
                    request += client.recv(1024)
                    print("Received form data after \\r\\n\\r\\n(i.e. from Safari on macOS or iOS)")
                except OSError:
                    pass

                print("Request is: {}".format(request))
                if "HTTP" not in request:  # skip invalid requests
                    continue

                # version 1.9 compatibility
                try:
                    url = re.search("(?:GET|POST) /(.*?)(?:\\?.*?)? HTTP", request).group(1).decode("utf-8").rstrip("/")
                except Exception:
                    url = re.search("(?:GET|POST) /(.*?)(?:\\?.*?)? HTTP", request).group(1).rstrip("/")
                print("URL is {}".format(url))

                if "generate_204" in url or "hotspot-detect" in url or "msftconnecttest" in url:
                    handle_redirect(client)
                elif url == "":
                    handle_root(client)
                elif url == "configure":
                    handle_configure(client, request)
                else:
                    handle_not_found(client, url)

            finally:
                client.close()

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
            print("ssid: %s chan: %d rssi: %d authmode: %s" % (ssid, channel, rssi, AUTHMODE.get(authmode, '?')))
            if encrypted:
                if ssid in profiles:
                    password = profiles[ssid]
                    connected = do_connect(ssid, password)
                else:
                    print("skipping unknown encrypted network")
            else:  # open
                connected = do_connect(ssid, None)
            if connected:
                break

    except OSError as e:
        print("exception", str(e))

    # start web server for connection manager:
    if not connected:
        connected = websrv_start()

    return wlan_sta if connected else None
