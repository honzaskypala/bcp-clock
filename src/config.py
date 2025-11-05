import json
import asyncio

config = {}

def load_config(**kwargs):
    DEFAULT_CONFIG = {
        "event": "",
        "yellow": 600,
        "red": 0,
        "refreshinterval": 60,
        "countdowninterval": 1,
        "hostname": "bcp-timer"
    }
    filename = kwargs.get("filename", f"{__name__}.json")
    global config
    try:
        with open(filename, "rb") as f:
            data = json.loads(f.read())
        config = merge_defaults(data, DEFAULT_CONFIG)
        return
    except OSError:
        pass
    config = DEFAULT_CONFIG.copy()

def merge_defaults(data, defaults):
    for key, value in defaults.items():
        if key not in data:
            data[key] = value
        elif isinstance(value, dict):
            data[key] = merge_defaults(data.get(key, {}), value)
    return data

def save_config(**kwargs):
    filename = kwargs.get("filename", f"{__name__}.json")
    with open(filename, "wb") as f:
        f.write(json.dumps(config))

http_config_updated = False

async def http_server():

    async def handle_client(reader, writer):

        def parse_qs(data):
            result = {}
            for pair in data.split("&"):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    k = k.replace("+", " ")
                    v = v.replace("+", " ")
                    def decode(s):
                        out = ""
                        i = 0
                        while i < len(s):
                            if s[i] == "%" and i + 2 < len(s):
                                try:
                                    out += chr(int(s[i+1:i+3], 16))
                                    i += 3
                                except:
                                    out += s[i]
                                    i += 1
                            else:
                                out += s[i]
                                i += 1
                        return out
                    result[decode(k)] = decode(v)
            return result

        def time_to_seconds(t):
            t = t.strip().replace(".", ":")     # if time entered as 05.00, replace dots by colons
            parts = [int(p) for p in t.strip().split(":")]
            if len(parts) == 2:      # mm:ss
                m, s = parts
                return m * 60 + s
            elif len(parts) == 3:    # hh:mm:ss
                h, m, s = parts
                return h * 3600 + m * 60 + s
            else:
                raise ValueError(f"Invalid time value: {t}")

        def seconds_to_time(seconds):
            h, m = divmod(seconds, 3600)
            m, s = divmod(m, 60)
            if h:
                return f"{h}:{m:02}:{s:02}"
            else:
                return f"{m:02}:{s:02}"

        def css():
            try:
                with open(f'{__name__}.css', 'r') as file:
                    return file.read()
            except FileNotFoundError:
                return ''

        def html(**kwargs):
            filename = kwargs.get('filename', f'{__name__}.html')
            status = kwargs.get('status', '')
            try:
                with open(filename, 'r') as file:
                    html = file.read()
            except FileNotFoundError:
                return ''
            global config
            html = html.format( css               = css(),
                                status            = status,
                                event             = config["event"],
                                yellow            = seconds_to_time(config["yellow"]),
                                red               = seconds_to_time(config["red"]),
                                refreshinterval   = config["refreshinterval"],
                                countdowninterval = config["countdowninterval"],
                                hostname          = config["hostname"]
                              )
            return html

        try:
            data = await reader.read(1024)
            if not data:
                await writer.aclose()
                return

            request = data.decode()
            response = html()

            if request.startswith("POST"):
                parts = request.split("\r\n\r\n", 1)
                if len(parts) > 1:
                    body = parts[1]
                    params = parse_qs(body)
                    global config, http_config_updated
                    for key, value in params.items():
                        if value and key in ("yellow", "red"):
                            config[key] = time_to_seconds(value)
                        elif value and key in ("event", "hostname"):
                            config[key] = value
                    http_config_updated = True
                    save_config()
                    response = html(status="Configuration saved", filename="config-success.html")

            writer.write(response.encode())
            await writer.drain()

        except Exception as e:
            print("Client error:", e)
        finally:
            await writer.aclose()

    server = await asyncio.start_server(handle_client, "0.0.0.0", 80)
    print("HTTP server running on port 80")

    async with server:
        while True:
            await asyncio.sleep(1)
