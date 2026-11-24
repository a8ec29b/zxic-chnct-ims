#!/bin/sh
set -e

root=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
adb=$root/platform-tools/adb
if [ ! -x "$adb" ]; then adb=adb; fi
if [ "$("$adb" get-state)" != device ]; then
    echo "请通过 ADB 连接一台设备。" >&2
    exit 1
fi

"$adb" shell 'mkdir -p /tmp/zxct-deploy'
"$adb" push "$root/deploy/uninstall-autostart.sh" /tmp/zxct-deploy/uninstall-autostart.sh
"$adb" shell 'sh /tmp/zxct-deploy/uninstall-autostart.sh'
echo '卸载完成。短信数据库和短信功能开关已保留。'
echo '若程序仍在运行，重启设备后会停止。'
