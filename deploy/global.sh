#!/bin/sh

if mkdir /tmp/zxct-ims-autostart 2>/dev/null; then
    (
        trap '' HUP
        log=/tmp/zxct-ims-autostart.log
        echo "autostart begin" > "$log"
        if [ -f /mnt/userdata/zxct-ims.gz ]; then
            gunzip -c /mnt/userdata/zxct-ims.gz > /tmp/zxct-ims || exit 1
            chmod 700 /tmp/zxct-ims
            program=/tmp/zxct-ims
        else
            program=/mnt/userdata/zxct-ims
        fi
        sh /mnt/userdata/zxct-ims-start/webui.sh
        "$program" --webui &

        i=0
        while [ "$i" -lt 60 ]; do
            ip route 2>/dev/null | grep -q '^default' && break
            sleep 3
            i=$(expr "$i" + 1)
        done
        echo "wan wait $i ticks" >> "$log"

        active_pid() {
            pid=$(cat /tmp/zxct-ims.pid 2>/dev/null)
            case "$pid" in ''|*[!0-9]*) return 1 ;; esac
            [ "$pid" -gt 1 ] && kill -0 "$pid" 2>/dev/null &&
                ps | awk -v p="$pid" '$1 == p && index($0, "zxct-ims --daemon") { found = 1 } END { exit !found }'
        }

        attempt=1
        while [ "$attempt" -le 6 ]; do
            if active_pid; then
                echo "existing process found; stop autostart" >> "$log"
                exit 0
            fi
            rm -f /tmp/zxct-ims.status /tmp/zxct-ims.pid
            echo "start attempt $attempt" >> "$log"
            nv get admin_Password | "$program" --daemon
            result=$?
            if [ "$result" -eq 3 ]; then
                echo "unsupported operator; stop autostart" >> "$log"
                exit 0
            fi
            owned_pid=
            j=0
            while [ "$j" -lt 60 ]; do
                sleep 2
                if active_pid; then
                    if [ -z "$owned_pid" ]; then owned_pid=$pid; fi
                    if [ "$pid" != "$owned_pid" ]; then
                        echo "process replaced; stop autostart" >> "$log"
                        exit 0
                    fi
                    if grep -q '^registered$' /tmp/zxct-ims.status 2>/dev/null; then
                        echo "registered on attempt $attempt" >> "$log"
                        exit 0
                    fi
                elif [ -n "$owned_pid" ] || [ "$result" -ne 0 ] || [ "$j" -ge 4 ]; then
                    echo "process exited on attempt $attempt" >> "$log"
                    break
                fi
                j=$(expr "$j" + 1)
            done
            if [ -n "$owned_pid" ] && active_pid && [ "$pid" = "$owned_pid" ]; then
                kill "$owned_pid" 2>/dev/null
                j=0
                while kill -0 "$owned_pid" 2>/dev/null && [ "$j" -lt 10 ]; do
                    sleep 1
                    j=$(expr "$j" + 1)
                done
                if kill -0 "$owned_pid" 2>/dev/null; then
                    echo "process did not stop; stop autostart" >> "$log"
                    exit 1
                fi
            fi
            attempt=$(expr "$attempt" + 1)
            [ "$attempt" -le 6 ] && sleep 10
        done
        echo "giving up" >> "$log"
    ) </dev/null >/dev/null 2>&1 &
fi

path_sh=/sbin
. /sbin/global.sh
path_sh=/sbin
