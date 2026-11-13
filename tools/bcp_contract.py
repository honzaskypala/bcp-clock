#!/usr/bin/env python3
# Contract test of the unofficial Best Coast Pairings API used by the firmware (lib/bcp, lib/core/bcpparse)
# (c) 2025 Honza Skýpala
# WTFPL license applies
#
# usage: ~/.platformio/penv/bin/python tools/bcp_contract.py <eventId> [--dump DIR]
# Run manually; it contacts the live API. --dump saves the responses as samples for the native tests.

import argparse
import json
import os
import re
import ssl
import sys
import urllib.error
import urllib.request

API = "https://newprod-api.bestcoastpairings.com"
ISO_UTC = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")   # format parsed by parseIsoUtc()

failures = 0

try:
    import certifi   # the PlatformIO Python has no system CA certificates
    SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())
except ImportError:
    SSL_CONTEXT = ssl.create_default_context()


def get(path):
    req = urllib.request.Request(API + path, headers={"client-id": "web-app", "User-Agent": "ESP32HTTPClient"})  # same as the firmware
    try:
        with urllib.request.urlopen(req, timeout=20, context=SSL_CONTEXT) as resp:
            return resp.status, resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")


def check(condition, message):
    global failures
    print(("  ok    " if condition else "  FAIL  ") + message)
    if not condition:
        failures += 1


def is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def check_timer(round_no, body):
    print(f"/v1/events/<id>/timer?round={round_no}")
    if body == {}:
        check(True, "{} (round without timer)")
        return
    check(is_int(body.get("timerLength")), "timerLength is int")
    for key in ("startTime", "endTime"):
        check(isinstance(body.get(key), str) and ISO_UTC.match(body[key]) is not None,
              f"{key} matches YYYY-MM-DDTHH:MM:SSZ ({body.get(key)!r})")
    if "timerPaused" in body:
        check(isinstance(body["timerPaused"], bool), "timerPaused is bool")
        if body["timerPaused"]:
            check(is_int(body.get("pausedTimeRemaining")), "pausedTimeRemaining is int")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("event_id")
    parser.add_argument("--dump", metavar="DIR", help="save the responses to DIR")
    args = parser.parse_args()

    print("/v2/events/<id>")
    status, text = get(f"/v2/events/{args.event_id}")
    check(status == 200, f"HTTP 200 (got {status})")
    if status != 200:
        return
    overview = json.loads(text)
    check(isinstance(overview.get("name"), str), "name is string")
    st = overview.get("status", {})
    for key in ("started", "ended"):
        check(isinstance(st.get(key), bool), f"status.{key} is bool")
    for key in ("numberOfRounds", "currentRound"):
        check(is_int(st.get(key)), f"status.{key} is int")
    responses = {"overview.json": text}

    rounds = sorted({max(1, st.get("currentRound") or 1), 999})
    for r in rounds:
        status, text = get(f"/v1/events/{args.event_id}/timer?round={r}")
        check(status == 200, f"/timer?round={r}: HTTP 200 (got {status})")
        if status == 200:
            body = json.loads(text)
            check_timer(r, body)
            responses[f"timer_round{r}.json"] = text
    check(json.loads(responses.get("timer_round999.json", "null")) == {}, "non-existent round returns {}")

    print("invalid event ID")
    status, _ = get("/v2/events/doesNotExist000")
    check(status == 404, f"HTTP 404 (got {status})")

    if args.dump:
        os.makedirs(args.dump, exist_ok=True)
        for name, text in responses.items():
            with open(os.path.join(args.dump, name), "w") as f:
                f.write(text)
        print(f"responses saved to {args.dump}")


if __name__ == "__main__":
    main()
    print("PASSED" if failures == 0 else f"FAILED ({failures})")
    sys.exit(1 if failures else 0)
