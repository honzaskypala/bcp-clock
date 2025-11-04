import json

config = {}

def load_config(**kwargs):
    DEFAULT_CONFIG = {
        "event": "nQrMXM3ykQID",    # fsRXYbsuNaWX: The Ones Left Behind on 11th November
        "yellow": 600,
        "red": 0,
        "refreshinterval": 60,
        "countdowninterval": 1,
        "hostname": "bcp-timer"
    }
    filename = kwargs.get("filename", f"{__name__}.json")
    try:
        with open(filename, "rb") as f:
            data = json.loads(f.read())
        return merge_defaults(data, DEFAULT_CONFIG)
    except OSError:
        pass
    return DEFAULT_CONFIG.copy()

def merge_defaults(data, defaults):
    for key, value in defaults.items():
        if key not in data:
            data[key] = value
        elif isinstance(value, dict):
            data[key] = merge_defaults(data.get(key, {}), value)
    return data

def save_config(config, **kwargs):
    filename = kwargs.get("filename", f"{__name__}.json")
    with open(filename, "wb") as f:
        f.write(json.dumps(config))
