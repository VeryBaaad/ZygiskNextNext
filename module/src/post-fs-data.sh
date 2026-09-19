#!/system/bin/sh

MODDIR=${0%/*}

cd "$MODDIR" || exit 1

mkdir -p /data/adb/zygisknextsu

./bin/injector "$MODDIR" &
