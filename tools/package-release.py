#!/usr/bin/env python3
import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parent.parent
DEPLOY = ("global.sh", "hook.sh", "install-autostart.sh", "uninstall-autostart.sh", "webui.sh", "webui.js")


def add(archive, name, data, executable=False):
    info = ZipInfo(name)
    info.create_system = 3
    info.external_attr = (0o100755 if executable else 0o100644) << 16
    info.compress_type = ZIP_DEFLATED
    archive.writestr(info, data)


def package(platform, tools_zip, output):
    with ZipFile(tools_zip) as tools, ZipFile(output, "w") as archive:
        def file(name, source, executable=False):
            add(archive, name, (ROOT / source).read_bytes(), executable)

        file("zxct-ims.gz", "build/zxct-ims.arm.gz")
        for name in DEPLOY:
            file(f"deploy/{name}", f"deploy/{name}")
        if platform == "linux":
            for name in ("install.sh", "uninstall.sh"):
                file(name, name, True)
            add(archive, "platform-tools/adb", tools.read("platform-tools/adb"), True)
        else:
            for name in ("install.cmd", "uninstall.cmd", "deploy/host-windows.ps1"):
                file(name, name)
            for name in ("adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll"):
                add(archive, f"platform-tools/{name}", tools.read(f"platform-tools/{name}"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--linux-platform-tools", type=Path, required=True)
    parser.add_argument("--windows-platform-tools", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build" / "release")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for platform, source in (("linux", args.linux_platform_tools), ("windows", args.windows_platform_tools)):
        output = args.output_dir / f"zxic-chnct-ims-{platform}.zip"
        package(platform, source, output)
        print(output)


if __name__ == "__main__":
    main()
