#!/bin/sh
# Keep the vendor page intact and mount a temporary copy for this boot.
page=/tmp/zxct-ims-index.html
script=/mnt/userdata/zxct-ims-start/webui.js
if grep -q '<!-- zxct-ims-injection -->' /etc_ro/web/index.html; then
    umount /etc_ro/web/index.html || exit 1
fi
[ -r "$script" ] || exit 1
awk -v script="$script" '
    {
        position = index($0, "</body>")
        if (position && !inserted) {
            printf "%s<!-- zxct-ims-injection --><script>\n", substr($0, 1, position - 1)
            while ((getline line < script) > 0) print line
            close(script)
            printf "</script>\n%s\n", substr($0, position)
            inserted = 1
        } else print
    }
    END { if (!inserted) exit 1 }
' /etc_ro/web/index.html > "$page.next" || exit 1
mv "$page.next" "$page" || exit 1
mount -o bind "$page" /etc_ro/web/index.html
