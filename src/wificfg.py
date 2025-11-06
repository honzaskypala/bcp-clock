# Wi-Fi configuration web
# (c) 2025 Honza Skýpala
# WTFPL license applies
# adapted from https://github.com/tayfunulu/WiFiManager by tayfunulu

import network
import time
import wifimgr
import socket
import re
from microDNS import MicroDNSSrv

def websrv_start(wlan_sta, kwargs):

    port = kwargs.get('port', 80)
    devicename = kwargs.get('devicename', "ESP-32")
    fb = kwargs.get('fb', None)
    ap_ssid = kwargs.get('ssid', "ESP-32")
    ap_password = kwargs.get('password', "")
    ap_authmode = 3  # WPA2

    wlan_ap = network.WLAN(network.AP_IF)

    def send_header(client, status_code=200, content_length=None ):
        client.sendall(f"HTTP/1.0 {status_code} OK\r\n")
        client.sendall("Content-Type: text/html\r\n")
        if content_length is not None:
            client.sendall(f"Content-Length: {content_length}\r\n")
        client.sendall("\r\n")

    def send_response(client, payload, status_code=200):
        content_length = len(payload)
        send_header(client, status_code, content_length)
        if content_length > 0:
            client.sendall(payload)
        client.close()

    def get_css():
        try:
            with open(f'{__name__}.css', 'r') as file:
                return file.read()
        except FileNotFoundError:
            return ''

    def handle_redirect(client):
        html = f"<html><meta name='viewport' content='width=device-width, initial-scale=1.0'><meta http-equiv='refresh' content='0; url=http://{wlan_ap.ifconfig()[0]}'>Redirecting, please wait...</html>"
        send_response(client, html)

    def handle_root(client):
        nonlocal wlan_sta, devicename
        css = get_css()
        wlan_sta.active(True)
        ssids = sorted(ssid.decode('utf-8') for ssid, *_ in wlan_sta.scan())
        send_header(client)
        client.sendall(f'<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1>{devicename} Wi-Fi Setup</h1><form action="configure" method="post">')
        if len(ssids):
            while len(ssids):
                ssid = ssids.pop(0)
                if ssid != "":
                    client.sendall(f'<div class="ssid"><input type="radio" name="ssid" value="{ssid}" id="{ssid}"/><label for="{ssid}">{ssid}</label></div>')
            client.sendall('<label for="password">Password:</label><input name="password" type="password" id="password" /><input type="submit" /></form></html>')
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

        nonlocal wlan_ap, devicename, fb
        css = get_css()
        if wifimgr.do_connect(wlan_sta, ssid, password, fb):
            response = f'<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1 class="success">{devicename} successfully connected to WiFi network {ssid}</h1></html>'
            send_response(client, response)
            time.sleep(1)
            wlan_ap.active(False)
            try:
                profiles = wifimgr.read_profiles()
            except OSError:
                profiles = {}
            profiles[ssid] = password
            wifimgr.write_profiles(profiles)

            time.sleep(5)

            return True
        else:
            response = f'<html><head class="error"><meta name="viewport" content="width=device-width, initial-scale=1.0"><style>{css}</style></head><h1 class="error">{devicename} could not connect to WiFi network {ssid}</h1><form><input type="button" value="Go back!" onclick="history.back()"></input></form></html>'
            send_response(client, response)
            return False

    def handle_not_found(client, url):
        send_response(client, f"Path not found: {url}", status_code=404)

    def websrv_stop():
        nonlocal server_socket
        if server_socket:
            server_socket.close()
            server_socket = None

    addr = socket.getaddrinfo('0.0.0.0', port)[0][-1]

    server_socket = None
    websrv_stop()

    wlan_sta.active(True)
    wlan_ap.active(True)

    wlan_ap.config(essid=ap_ssid, password=ap_password)

    server_socket = socket.socket()
    server_socket.bind(addr)
    server_socket.listen(1)

    mdns = MicroDNSSrv.Create({ '*' : wlan_ap.ifconfig()[0] })

    print(f'Connect to WiFi ssid {ap_ssid}, default password: {ap_password}')
    print(f'If a config page does not open automatically, access the ESP via your favorite web browser at {wlan_ap.ifconfig()[0]}.')

    if fb:
        fb.clear()
        fb.text("AP mode", 0, 1, font="f3x5", centered=True)
        fb.show()

    while True:
        if wlan_sta.isconnected():
            mdns.Stop()
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

            print(f"Request is: {request}")
            if "HTTP" not in request:  # skip invalid requests
                continue

            # version 1.9 compatibility
            try:
                url = re.search("(?:GET|POST) /(.*?)(?:\\?.*?)? HTTP", request).group(1).decode("utf-8").rstrip("/")
            except Exception:
                url = re.search("(?:GET|POST) /(.*?)(?:\\?.*?)? HTTP", request).group(1).rstrip("/")
            print(f"URL is {url}")

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