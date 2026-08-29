#!/system/bin/sh

MODDIR=${0%/*}

cd "$MODDIR"

mkdir -p /data/adb/zygisknextsu

./bin/injector "$MODDIR" &
