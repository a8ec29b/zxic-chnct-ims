#!/bin/sh
set -e

dir=/mnt/userdata/zxct-ims-start
current=$(nv get path_sh)
[ -x /mnt/userdata/zxct-ims ] || [ -f /mnt/userdata/zxct-ims.gz ]
mkdir -p "$dir" /tmp/zxct-ims-autostart
cp "$(dirname "$0")/webui.sh" "$dir/webui.sh"
cp "$(dirname "$0")/webui.js" "$dir/webui.js"
if [ "$current" = /sbin ] || [ "$current" = "$dir" ]; then
    cp "$(dirname "$0")/global.sh" "$dir/global.sh.next"
    chmod 700 "$dir/global.sh.next"
    mv "$dir/global.sh.next" "$dir/global.sh"
    nv set "path_sh=$dir"
    [ "$(nv get path_sh)" = "$dir" ]
else
    script=$current/global.sh
    [ -f "$script" ] && [ ! -L "$script" ] && [ -w "$script" ]
    cp -p "$script" "$script.next"
    sed '/^# zxct-ims startup begin$/,/^# zxct-ims startup end$/d' "$script" > "$script.clean"
    { head -n 1 "$script.clean"; cat "$(dirname "$0")/hook.sh"; sed '1d' "$script.clean"; } > "$script.next"
    rm -f "$script.clean"
    mv "$script.next" "$script"
fi
if [ "$(nv get need_support_sms)" = no ]; then
    nv set need_support_sms=yes
    [ "$(nv get need_support_sms)" = yes ]
fi
nv save
sync
echo "Startup entry installed; it will run after the next reboot."
