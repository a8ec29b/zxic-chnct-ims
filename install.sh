#!/bin/sh
set -e

root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
adb=$root/platform-tools/adb
if [ ! -x "$adb" ]; then adb=adb; fi
program=$root/zxct-ims.gz
if [ ! -f "$program" ]; then program=$root/build/zxct-ims.arm.gz; fi

if [ ! -f "$program" ]; then
    echo "找不到程序文件，请使用完整的部署包或先构建程序。" >&2
    exit 1
fi
if [ "$("$adb" get-state)" != device ]; then
    echo "请通过 ADB 连接一台设备。" >&2
    exit 1
fi

"$adb" push "$program" /mnt/userdata/zxct-ims.gz
"$adb" shell 'mkdir -p /tmp/zxct-deploy'
for file in global.sh hook.sh install-autostart.sh webui.sh webui.js; do
    "$adb" push "$root/deploy/$file" "/tmp/zxct-deploy/$file"
done
"$adb" shell 'sh /tmp/zxct-deploy/install-autostart.sh'
echo '安装完成。重启设备后程序将自动启动，重启会短暂断网。'
printf '现在重启设备？[y/N] '
read -r answer || answer=
case "$answer" in y|Y|yes|YES) "$adb" reboot ;; esac
