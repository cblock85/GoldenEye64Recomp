#!/bin/bash
# Test harness: boot straight into a level, skip the intro, hold a look angle, screenshot the window.
#
# Usage: tools/harness.sh <level-id> <look-degrees> <out.png> [wait-seconds] [binary]
#   level ids: 26 Frigate, 33 Dam, 34 Facility, 35 Runway, 36 Surface (LEVELID_* in patches/structs.h)
#   look:      vertical look angle in degrees, e.g. 30 to look up at the sky, 0 straight ahead
#   binary:    defaults to build/GoldenRecomp; point it at another build to compare
#   GE_HARNESS_SKIP=0 in the environment lets the intro play out instead of skipping it.
#   GE_HARNESS_BUTTONS=<mask> holds N64 buttons on controller 1, e.g. 0x0010 = R (aim).
#
# The ROM must already be set up (picked once in the launcher). Needs spectacle (KDE).
set -e
cd "$(dirname "$0")/.."

STAGE=${1:?usage: tools/harness.sh <level-id> <look-degrees> <out.png> [wait-seconds] [binary]}
LOOK=${2:?look angle missing}
OUT=${3:?output png missing}
WAIT=${4:-20}
BIN=${5:-./build/GoldenRecomp}
LOG="${OUT%.png}.log"

GE_HARNESS_STAGE=$STAGE GE_HARNESS_LOOK=$LOOK "$BIN" >"$LOG" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null || true' EXIT

sleep "$WAIT"
if ! kill -0 $PID 2>/dev/null; then
    echo "game exited early, see $LOG" >&2
    exit 1
fi

# spectacle can only capture the active window, so bring the game's window to the front first (KWin on
# Wayland has no other way to address a window). Without KWin this is skipped and the shot may be wrong
# if another window has focus.
if command -v qdbus >/dev/null 2>&1; then
    SCRIPT=$(mktemp --suffix=.js)
    cat >"$SCRIPT" <<'EOF'
for (const w of workspace.windowList()) {
    if (w.caption.indexOf("Goldeneye 007") === 0) {
        workspace.activeWindow = w;
    }
}
EOF
    qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SCRIPT" ge-harness-activate >/dev/null 2>&1 &&
        qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.start >/dev/null 2>&1
    qdbus org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript ge-harness-activate >/dev/null 2>&1
    rm -f "$SCRIPT"
    sleep 0.5
fi

spectacle --background --nonotify --activewindow --output "$OUT"
echo "$OUT"
