# zxic-chnct-ims

在使用**中兴微（ZXIC）方案**的随身 Wi-Fi 设备内部，通过中国电信 IMS 接收短信，并将短信写入设备自带的短信数据库。这里的中兴微方案不等同于中兴通讯（ZTE）设备；固件中的 `zte_*` 名称是原厂程序或库的名称。

项目只实现中国电信 IMS 短信接收。它优先使用设备内部 AT 接口，必要时使用原厂 Web AT 接口；SIM 鉴权命令在结果不明确时不会跨接口重发。无线驻网和频段选择由调制解调器处理。

## 状态与限制

- 已在部分设备上验证 IMS 注册、重启自启动和普通短信接收；收到的短信可在原厂界面显示为未读。不同固件的兼容性仍需验证。
- 已在原厂空短信库设备上创建短信表并写入模拟短信，原厂界面显示正常。
- 长短信的真实网络接收、注册续期、断线恢复和部分界面操作尚未完成实机验证。
- 短信按设备原有格式明文保存在设备数据库中。IMS 注册和报文完整性按运营商协商实现；当前仅支持 `null` 加密的 ESP，实际协商由网络决定。
- 程序仅在确认中国电信 SIM 后访问短信库和 IMS。若 SIM 在启动时尚未就绪，进程可能退出。

## 构建

主机需要 C 编译器、SQLite 开发头文件和 `make`：

```sh
make
make test
```

针对设备构建需要 Linux 构建环境（Windows 可使用 WSL）、ARMv7 软浮点 uClibc 交叉工具链，以及与目标固件兼容的 SQLite、pthread、dl 和 libc 运行库。请自行从目标设备准备这些库；本仓库不包含原厂固件或运行库。

```sh
make arm \
  ARM_CC=/path/to/arm-buildroot-linux-uclibcgnueabi-gcc \
  ARM_STRIP=/path/to/arm-buildroot-linux-uclibcgnueabi-strip \
  ARM_SYSROOT_LIB=/path/to/target-libraries
```

构建得到 `build/zxct-ims.arm` 和默认安装用的 `build/zxct-ims.arm.gz`。如果目标 SQLite 头文件不在 `/usr/include/sqlite3.h`，可通过 `SQLITE_HEADER` 指定。

## 安装与运行

先确认设备已连接 ADB、插入中国电信卡，并有足够的可写空间。ADB shell 需要能够写入 `/mnt/userdata` 并执行设备上的 `nv` 命令。示例将压缩程序放在设备的持久存储中，将安装脚本放在临时目录：

```sh
adb push build/zxct-ims.arm.gz /mnt/userdata/zxct-ims.gz
adb shell 'mkdir -p /tmp/zxct-deploy'
adb push deploy/global.sh /tmp/zxct-deploy/global.sh
adb push deploy/hook.sh /tmp/zxct-deploy/hook.sh
adb push deploy/install-autostart.sh /tmp/zxct-deploy/install-autostart.sh
adb push deploy/uninstall-autostart.sh /tmp/zxct-deploy/uninstall-autostart.sh
adb shell 'sh /tmp/zxct-deploy/install-autostart.sh'
```

安装程序保留已有的可写启动脚本，并在原厂短信开关为 `no` 时将其设为 `yes`。重启后，启动脚本将程序解压到设备内存中的 `/tmp/zxct-ims` 并启动。卸载启动入口可执行 `/tmp/zxct-deploy/uninstall-autostart.sh`；它不会删除短信库。

### Windows 部署

Windows 电脑可以使用 [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools) 中的 ADB 部署，必要时安装设备的 Windows ADB 驱动。安装脚本通过 `adb shell` 在设备内运行，电脑部署时不需要 WSL。可从项目 Releases 下载部署包并解压，或在 Linux 或 WSL 中按上面的步骤构建。部署目录需要包含 `build/zxct-ims.arm.gz` 和 `deploy` 目录中的脚本。

将 Platform-Tools 加入 PATH，在解压后的部署目录或项目根目录打开 PowerShell，执行：

```powershell
adb devices
adb push .\build\zxct-ims.arm.gz /mnt/userdata/zxct-ims.gz
adb shell "mkdir -p /tmp/zxct-deploy"
adb push .\deploy\global.sh /tmp/zxct-deploy/global.sh
adb push .\deploy\hook.sh /tmp/zxct-deploy/hook.sh
adb push .\deploy\install-autostart.sh /tmp/zxct-deploy/install-autostart.sh
adb push .\deploy\uninstall-autostart.sh /tmp/zxct-deploy/uninstall-autostart.sh
adb shell "sh /tmp/zxct-deploy/install-autostart.sh"
```

`adb devices` 应将设备状态显示为 `device`；如果显示 `unauthorized`，需先在设备上完成 ADB 授权。安装后重新启动设备会短暂断网，启动完成后可用 `adb shell "cat /tmp/zxct-ims.status"` 和 `adb shell "ps | grep zxct-ims"` 检查状态与进程。

后台状态位于 `/tmp/zxct-ims.status`：`registered` 为已注册，`starting` 和 `waiting` 表示尚未就绪。检查时也应确认进程存在。前台调试时，可先解压并运行程序，从标准输入输入原厂后台密码；设备管理终端可能回显输入，应使用可信终端。

## 参考与许可证

项目代码采用 [Apache-2.0](LICENSE)。第三方组件、参考项目、许可证及待核实事项见 [第三方说明](THIRD_PARTY_NOTICES.md)；设备验证范围见 [设备记录](docs/device-notes.md)，审计范围和已知风险见 [审计记录](docs/security-review.md)。
