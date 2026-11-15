# BCP API

- `GET https://newprod-api.bestcoastpairings.com/v2/events/<id>` → `name`, `status.started/ended/numberOfRounds/currentRound`.
  The response is ~6 KB; `parseOverview` uses an ArduinoJson filter to keep only `name` and `status`.
  Spec: https://github.com/honzaskypala/bcp-api/blob/main/bcp-api.yaml
- `GET .../v1/events/<id>/timer?round=<n>` (no v2 exists) → `timerLength`, `startTime`, `endTime` (UTC ISO), `timerPaused`, `pausedTimeRemaining`.
  A round without a timer (also a non-existent round) returns `200 {}`, not 404. BCP has no way to cancel a started timer.
- Header `client-id: web-app`; TLS without cert validation (`setInsecure()`). Unofficial API.
- Responses are parsed directly from the HTTP stream (`parseOverview`/`parseTimer` `Stream&` overloads), no body
  `String`. The request is HTTP/1.0 (`useHTTP10`), so the body is never chunked (BCP sends `Content-Length`).
- 404 on the event (v2) = invalid event ID (counts as a "successful" fetch, not a failure).
- Contract test of the live API: `python tools/bcp_contract.py <eventId>` (`--dump` saves responses).
