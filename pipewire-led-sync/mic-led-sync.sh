#!/bin/bash
set -euo pipefail

LED_PATH="/sys/class/leds/platform::micmute"
BRIGHTNESS="$LED_PATH/brightness"
TRIGGER="$LED_PATH/trigger"

# Validate LED device exists
if [[ ! -d "$LED_PATH" ]]; then
    echo "ERROR: LED device not found at $LED_PATH" >&2
    exit 1
fi

# Validate writable
if [[ ! -w "$BRIGHTNESS" || ! -w "$TRIGGER" ]]; then
    echo "ERROR: $BRIGHTNESS or $TRIGGER not writable (udev rule missing?)" >&2
    exit 1
fi

# Validate pactl available
if ! command -v pactl &>/dev/null; then
    echo "ERROR: pactl not found" >&2
    exit 1
fi

restore_trigger() {
    echo "Restoring trigger to audio-micmute" >&2
    echo audio-micmute > "$TRIGGER" 2>/dev/null || true
}
trap restore_trigger EXIT

# Disable kernel trigger, take full control
echo none > "$TRIGGER"
echo "Took control of LED (trigger=none)" >&2

set_led() {
    # 1 = muted (LED on), 0 = unmuted (LED off)
    echo "$1" > "$BRIGHTNESS"
}

query_and_set() {
    local output
    if output=$(LANG=C pactl get-source-mute @DEFAULT_SOURCE@ 2>/dev/null); then
        case "$output" in
            *"Mute: yes"*) set_led 1 ;;
            *"Mute: no"*)  set_led 0 ;;
            *)
                echo "WARNING: unexpected pactl output: $output" >&2
                set_led 0
                ;;
        esac
    else
        echo "WARNING: pactl query failed, setting LED off" >&2
        set_led 0
        return 1
    fi
}

drain_events() {
    # Debounce: a single action (e.g. mute toggle) triggers a burst of events
    # from PipeWire. Consume and discard all events until 100ms of silence,
    # then make a single state query instead of one per event.
    # Cap at 1 second to prevent indefinite blocking from event floods.
    local deadline=$(( SECONDS + 1 ))
    while (( SECONDS < deadline )) && read -t 0.1 _discard; do
        :
    done
}

echo "Starting mic-led-sync daemon" >&2

while true; do
    # Query initial state
    if ! query_and_set; then
        sleep 2
        continue
    fi

    # Monitor events — pactl subscribe exits if PipeWire dies
    LANG=C pactl subscribe 2>/dev/null | while read -r line; do
        case "$line" in
            *"'change' on source #"*|*"'change' on server"*|*"'new' on source #"*|*"'remove' on source #"*)
                drain_events
                query_and_set || true
                ;;
        esac
    done || true

    echo "pactl subscribe exited, reconnecting in 2s..." >&2
    set_led 0
    sleep 2
done
