#!/bin/bash
#
# Counts how many apps are recording from the BluCast virtual microphone and
# writes the count to /tmp/blucast/audio_consumers — the audio analog of
# vcam_watcher.sh. The server can use this to idle the GPU model when nothing
# is listening. Runs on the host (where pactl talks to the user's audio server).

MIC_SOURCE="${1:-BluCast_Virtual_Microphone}"
CONSUMERS_FILE="/tmp/blucast/audio_consumers"

mkdir -p /tmp/blucast
echo "0" > "$CONSUMERS_FILE"

if ! command -v pactl &>/dev/null; then
    echo "Warning: pactl not available. Audio consumer detection disabled." >&2
    while true; do echo "0" > "$CONSUMERS_FILE"; sleep 5; done
    exit 0
fi

count_consumers() {
    # Resolve our source's numeric index, then count source-outputs bound to it.
    local idx
    idx=$(pactl list short sources 2>/dev/null \
        | awk -v s="$MIC_SOURCE" '$2 == s { print $1; exit }')
    [ -z "$idx" ] && { echo 0; return; }

    pactl list source-outputs 2>/dev/null | awk -v idx="$idx" '
        /^Source Output #/ { has = 0 }
        /^[[:space:]]*Source:[[:space:]]/ { if ($2 == idx) has = 1 }
        /^$/ { count += has; has = 0 }
        END { print count + 0 }
    '
}

while true; do
    n=$(count_consumers)
    [[ "$n" =~ ^[0-9]+$ ]] || n=0
    echo "$n" > "$CONSUMERS_FILE"
    sleep 1
done
