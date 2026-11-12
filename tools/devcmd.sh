#!/bin/sh
# Sends a console command to the dev build via tools/devmon.py and prints the response
# (c) 2025 Honza Skýpala
# WTFPL license applies
#
# usage: tools/devcmd.sh <command> [args...]    e.g. tools/devcmd.sh fake run 300

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LOG=$ROOT/.pio/dev.log
FIFO=$ROOT/.pio/dev.fifo

if [ ! -p "$FIFO" ]; then
    echo "devmon.py is not running (no $FIFO)" >&2
    exit 1
fi

START=$(wc -c < "$LOG")
echo "$*" > "$FIFO"
# wait up to 15 s for the console to finish the command
for i in $(seq 1 75); do
    tail -c +$((START + 1)) "$LOG" | grep -q '\[Console\] done' && break
    sleep 0.2
done
tail -c +$((START + 1)) "$LOG"
