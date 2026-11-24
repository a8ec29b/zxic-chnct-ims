# zxct-ims startup begin
if mkdir /tmp/zxct-ims-autostart 2>/dev/null; then
    (
        trap '' HUP
        sleep 30
        if [ -f /mnt/userdata/zxct-ims.gz ]; then
            gunzip -c /mnt/userdata/zxct-ims.gz > /tmp/zxct-ims || exit 1
            chmod 700 /tmp/zxct-ims
            program=/tmp/zxct-ims
        else
            program=/mnt/userdata/zxct-ims
        fi
        nv get admin_Password | "$program" --daemon
    ) </dev/null >/dev/null 2>&1 &
fi
# zxct-ims startup end
