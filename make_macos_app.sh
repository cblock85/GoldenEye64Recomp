#!/bin/bash
# Package GoldenRecomp as a double-clickable macOS app bundle.
# Run from the kit root after a successful build: ./make_macos_app.sh
# Produces "GoldenEye 007 Recompiled.app" in the kit root (drag it to /Applications if you like).
set -e
cd "$(dirname "$0")"

APP="GoldenEye 007 Recompiled.app"
BIN=build/GoldenRecomp

if [ ! -f "$BIN" ]; then
    echo "ERROR: $BIN not found — build first (./build_macos.sh or ninja -C build GoldenRecomp)"
    exit 1
fi

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

# --- Info.plist ---------------------------------------------------------------
cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>GoldenEye 007 Recompiled</string>
    <key>CFBundleDisplayName</key>       <string>GoldenEye 007 Recompiled</string>
    <key>CFBundleIdentifier</key>        <string>io.github.goldenrecomp</string>
    <key>CFBundleVersion</key>           <string>1.0</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>CFBundleExecutable</key>        <string>launch</string>
    <key>CFBundleIconFile</key>          <string>AppIcon</string>
    <key>NSHighResolutionCapable</key>   <true/>
    <key>LSMinimumSystemVersion</key>    <string>13.0</string>
</dict>
</plist>
EOF

# --- Launcher stub (sets cwd so runtime files resolve) ------------------------
cat > "$APP/Contents/MacOS/launch" <<'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"
cd "$DIR"
exec "$DIR/GoldenRecomp"
EOF
chmod +x "$APP/Contents/MacOS/launch"

# --- Payload ------------------------------------------------------------------
cp "$BIN" "$APP/Contents/Resources/GoldenRecomp"
cp gamecontrollerdb.txt "$APP/Contents/Resources/" 2>/dev/null || true
cp -R assets "$APP/Contents/Resources/assets"

# --- Icon (built with Apple's own tools) --------------------------------------
if command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
    ICONSET=$(mktemp -d)/AppIcon.iconset
    mkdir -p "$ICONSET"
    for sz in 16 32 64 128 256 512; do
        sips -z $sz $sz icons/512.png --out "$ICONSET/icon_${sz}x${sz}.png" >/dev/null
        dbl=$((sz*2))
        if [ $dbl -le 1024 ]; then
            sips -z $dbl $dbl icons/512.png --out "$ICONSET/icon_${sz}x${sz}@2x.png" >/dev/null
        fi
    done
    iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
    echo "Icon created."
else
    echo "NOTE: sips/iconutil not found; app will use a generic icon."
fi

echo ""
echo "Done: \"$APP\""
echo "Double-click it in Finder, or:  open \"$APP\""
echo "Config and the stored ROM now live in ~/Library/Application Support/GoldenRecomp"
echo "(first launch will ask for your ROM again — your retail .z64 works directly)."
