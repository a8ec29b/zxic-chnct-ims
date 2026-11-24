#!/bin/sh
set -e

dir=/mnt/userdata/zxct-ims-start
current=$(nv get path_sh)
[ -x /mnt/userdata/zxct-ims ] || [ -f /mnt/userdata/zxct-ims.gz ]
mkdir -p "$dir" /tmp/zxct-ims-autostart
if [ "$current" = /sbin ] || [ "$current" = "$dir" ]; then
    cp "$(dirname "$0")/global.sh" "$dir/global.sh.next"
    chmod 700 "$dir/global.sh.next"
    mv "$dir/global.sh.next" "$dir/global.sh"
    nv set "path_sh=$dir"
    [ "$(nv get path_sh)" = "$dir" ]
else
    script=$current/global.sh
    [ -f "$script" ] && [ ! -L "$script" ] && [ -w "$script" ]
    if ! grep -q '^# zxct-ims startup begin$' "$script"; then
        cp -p "$script" "$script.next"
        { head -n 1 "$script"; cat "$(dirname "$0")/hook.sh"; sed '1d' "$script"; } > "$script.next"
        mv "$script.next" "$script"
    fi
fi
if [ "$(nv get need_support_sms)" = no ]; then
    nv set need_support_sms=yes
    [ "$(nv get need_support_sms)" = yes ]
fi
nv save
sync
echo "Startup entry installed; it will run after the next reboot."
