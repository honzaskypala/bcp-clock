# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

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