# BCP Clock
# (c) 2025 Honza Skýpala
# WTFPL license applies

import json

def time_to_seconds(t):
    parts = [int(p) for p in t.strip().split(":")]
    if len(parts) == 2:      # mm:ss
        m, s = parts
        return m * 60 + s
    elif len(parts) == 3:    # hh:mm:ss
        h, m, s = parts
        return h * 3600 + m * 60 + s
    else:
        raise ValueError(f"Invalid time value: {t}")

class Config:
    _native_keys = ("event", "yellow", "red", "refreshinterval", "countdowninterval", "hostname")

    def __init__(self, filename="config.json"):
        self.load(filename)

    def save(self, filename="config.json"):
        with open(filename, "wb") as f:
            f.write(json.dumps(self._config))

    def load(self, filename="config.json"):
        try:
            with open(filename, "rb") as f:
                self._config = json.loads(f.read())
        except OSError:
            self._config = {
                "event": "fsRXYbsuNaWX",    # fsRXYbsuNaWX: The Ones Left Behind on 11th November
                "yellow": "10:00",
                "red": "00:00",
                "refreshinterval": 60,
                "countdowninterval": 1,
                "hostname": "bcp-timer"
            }

    def __getitem__(self, key):
        if key in Config._native_keys:
            return self._config[key]
        elif key == "yellowseconds":
            return time_to_seconds(self._config["yellow"])
        elif key == "redseconds":
            return time_to_seconds(self._config["red"])
        else:
            raise KeyError(f"Invalid config key: {key}")

    def __setitem__(self, key, value):
        if key in Config._native_keys:
            self._config[key] = value
        else:
            raise KeyError(f"Invalid config key: {key}")

    def _delitem__(self, key):
        raise NotImplementedError("Cannot delete config items")

