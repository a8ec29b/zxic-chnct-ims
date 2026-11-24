#!/bin/sh
set -e

dir=/mnt/userdata/zxct-ims-start
current=$(nv get path_sh)
if [ "$current" = "$dir" ]; then
    nv set path_sh=/sbin
    nv save
    [ "$(nv get path_sh)" = /sbin ]
elif [ "$current" != /sbin ]; then
    script=$current/global.sh
    if [ -f "$script" ] && [ ! -L "$script" ] && grep -q '^# zxct-ims startup begin$' "$script"; then
        cp -p "$script" "$script.next"
        sed '/^# zxct-ims startup begin$/,/^# zxct-ims startup end$/d' "$script" > "$script.next"
        mv "$script.next" "$script"
    fi
fi
rm -f "$dir/global.sh"
rmdir "$dir" 2>/dev/null || :
if command -v killall >/dev/null 2>&1; then killall zxct-ims 2>/dev/null || :; fi
rm -f /mnt/userdata/zxct-ims.gz /mnt/userdata/zxct-ims /tmp/zxct-ims /tmp/zxct-ims.status
rmdir /tmp/zxct-ims-autostart 2>/dev/null || :
echo "Startup entry and program removed."
