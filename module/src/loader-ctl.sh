#!/system/bin/sh

MODDIR=${0%/*}/..

case "${1:-status}" in
  rescan)
    pid=$(pidof injector 2>/dev/null)
    if [ -n "$pid" ]; then
      kill -HUP $pid && echo "injector ($pid) requested to rescan modules"
    else
      echo "injector is not running"
    fi
    ;;
  status)
    pid=$(pidof injector 2>/dev/null)
    if [ -n "$pid" ]; then
      echo "injector running (pid $pid)"
    else
      echo "injector is not running"
    fi
    ;;
  *)
    echo "usage: loader-ctl [rescan|status]"
    ;;
esac
