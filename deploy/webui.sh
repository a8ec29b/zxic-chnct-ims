#!/bin/sh
# Keep the vendor page intact and mount a temporary copy for this boot.
page=/tmp/zxct-ims-index.html
grep -q "</body>" /etc_ro/web/index.html || exit 1
sed '/<\/body>/,$d' /etc_ro/web/index.html > "$page" || exit 1
printf '<script>\n' >> "$page"
cat /mnt/userdata/zxct-ims-start/webui.js >> "$page" || exit 1
printf '\n</script>\n</body>\n</html>\n' >> "$page"
mount -o bind "$page" /etc_ro/web/index.html
