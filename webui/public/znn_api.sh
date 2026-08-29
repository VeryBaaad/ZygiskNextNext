#!/system/bin/sh

json_escape() {
  printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

PROC_ROOT=${ZNN_PROC_ROOT:-/proc}
MODULES_ROOT=${ZNN_MODULES_ROOT:-/data/adb/modules}

injector_pid() {
  pidof injector 2>/dev/null
}

zygisk_present() {
  for f in "$PROC_ROOT"/[0-9]*/comm; do
    [ -r "$f" ] || continue
    if grep -qi zygisk "$f" 2>/dev/null; then
      return 0
    fi
  done
  return 1
}

cmd_status() {
  pid=$(injector_pid)
  if [ -n "$pid" ]; then
    running=true
  else
    running=false
    pid=0
  fi
  if zygisk_present; then
    compat=true
  else
    compat=false
  fi
  printf '{"running":%s,"pid":%s,"zygiskCompat":%s}\n' "$running" "$pid" "$compat"
}

cmd_system() {
  kernel=$(uname -r 2>/dev/null)
  sdk=$(getprop ro.build.version.sdk 2>/dev/null)
  abi=$(getprop ro.product.cpu.abi 2>/dev/null)
  abilist=$(getprop ro.product.cpu.abilist 2>/dev/null)
  [ -z "$abi" ] && abi=$(getprop ro.product.cpu.abi2 2>/dev/null)

  ksu=""
  magisk=""
  apatch=""
  if [ -x /data/adb/ksud ]; then
    ksu=$(/data/adb/ksud --version 2>/dev/null | head -n 1)
  fi
  if [ -x /data/adb/magisk/magisk ]; then
    magisk=$(/data/adb/magisk/magisk -v 2>/dev/null | head -n 1)
    [ -z "$magisk" ] && magisk=$(/data/adb/magisk/magisk -V 2>/dev/null | head -n 1)
  fi
  if [ -x /data/adb/apd ]; then
    apatch=$(/data/adb/apd --version 2>/dev/null | head -n 1)
  fi

  sdk_num=0
  case "$sdk" in
    '' | *[!0-9]*) sdk_num=0 ;;
    *) sdk_num=$sdk ;;
  esac

  printf '{"kernel":"%s","sdk":%s,"abi":"%s","abilist":"%s","root":{"kernelSU":"%s","magisk":"%s","apatch":"%s"}}\n' \
    "$(json_escape "$kernel")" "$sdk_num" "$(json_escape "$abi")" "$(json_escape "$abilist")" \
    "$(json_escape "$ksu")" "$(json_escape "$magisk")" "$(json_escape "$apatch")"
}

cmd_modules() {
  tmp=$(mktemp 2>/dev/null) || tmp="/data/local/tmp/znn_procs.$$"
  : > "$tmp"
  for d in "$PROC_ROOT"/[0-9]*; do
    [ -d "$d" ] || continue
    ppid=${d##*/}
    maps="$d/maps"
    [ -r "$maps" ] || continue
    if grep -qE '/memfd:loader|libloader\.so' "$maps" 2>/dev/null; then
      pname=$(tr '\0' ' ' < "$d/cmdline" 2>/dev/null | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
      [ -z "$pname" ] && pname=$(cat "$d/comm" 2>/dev/null)
      [ -z "$pname" ] && pname="?"
      pexe=$(readlink "$d/exe" 2>/dev/null)
      printf '%s\t%s\t%s\n' "$ppid" "$pname" "$pexe" >> "$tmp"
    fi
  done

  first=1
  printf '['
  for mdir in "$MODULES_ROOT"/*/; do
    [ -d "$mdir" ] || continue
    [ -e "$mdir/disable" ] && continue
    [ -e "$mdir/remove" ] && continue
    [ -f "$mdir/zn_modules.txt" ] || continue

    mid=${mdir%/}
    mid=${mid##*/}

    mname=$(sed -n 's/^name=//p' "$mdir/module.prop" 2>/dev/null | head -n 1)
    [ -z "$mname" ] && mname=$mid
    mversion=$(sed -n 's/^version=//p' "$mdir/module.prop" 2>/dev/null | head -n 1)

    targets=""
    while IFS= read -r line; do
      case "$line" in
        path=*) targets="$targets path:${line#path=}" ;;
        name=*) targets="$targets name:${line#name=}" ;;
      esac
    done < "$mdir/zn_modules.txt"

    procs=""
    sep=""
    if [ -n "$targets" ]; then
      while IFS=$(printf '\t') read -r ppid pname pexe; do
        [ -n "$ppid" ] || continue
        hit=0
        for tg in $targets; do
          case "$tg" in
            path:*)
              [ "$pexe" = "${tg#path:}" ] && hit=1
              ;;
            name:*)
              want=${tg#name:}
              base=${pexe##*/}
              if [ "$base" = "$want" ] || [ "$pname" = "$want" ]; then
                hit=1
              fi
              ;;
          esac
          [ "$hit" = 1 ] && break
        done
        if [ "$hit" = 1 ]; then
          procs="$procs$sep{\"pid\":$ppid,\"name\":\"$(json_escape "$pname")\"}"
          sep=,
        fi
      done < "$tmp"
    fi

    if [ "$first" = 1 ]; then
      first=0
    else
      printf ','
    fi
    printf '{"id":"%s","name":"%s","version":"%s","processes":[%s]}' \
      "$(json_escape "$mid")" "$(json_escape "$mname")" "$(json_escape "$mversion")" "$procs"
  done
  printf ']'
  rm -f "$tmp"
}

case "$1" in
  status) cmd_status ;;
  system) cmd_system ;;
  modules) cmd_modules ;;
  *) printf 'usage: znn_api.sh <status|system|modules>\n' >&2 ; exit 1 ;;
esac
