#!/system/bin/sh

MODDIR=${0%/*}

cd "$MODDIR"

# Runtime directory (original Zygisk Next convention). The injector writes its
# state snapshot (/data/adb/zygisksu/znn_state.json) here for the WebUI.
mkdir -p /data/adb/zygisksu

./bin/injector "$MODDIR" &
