#!/system/bin/sh

MODDIR=${0%/*}

cd "$MODDIR"

./bin/injector "$MODDIR" &
