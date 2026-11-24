#!/bin/sh
# Builds every env of every build configuration (configs/*.ini): checks that each feature combination compiles
# (c) 2025 Honza Skýpala
# WTFPL license applies
#
# Usage: tools/build_configs.sh        Logs: .pio/configs/<env>.log

cd "$(dirname "$0")/.."

PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"

LOGDIR=.pio/configs
mkdir -p "$LOGDIR"

ENVS=$(sed -n 's/^\[env:\(.*\)\]$/\1/p' configs/*.ini)
FAILED=""
for env in $ENVS; do
    if "$PIO" run -e "$env" > "$LOGDIR/$env.log" 2>&1; then
        echo "$env: OK, $(grep -E '^Flash:' "$LOGDIR/$env.log" | sed 's/.*(used/used/;s/)//')"
    else
        echo "$env: FAILED (log: $LOGDIR/$env.log)"
        grep -E "error:|#error" "$LOGDIR/$env.log" | sed 's/^/    /'
        FAILED="$FAILED $env"
    fi
done

if [ -n "$FAILED" ]; then
    echo "Build failed:$FAILED"
    exit 1
fi
echo "All build configurations built"
