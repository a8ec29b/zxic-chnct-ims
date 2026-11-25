# zxic-chnct-ims

让中兴微随身 Wi-Fi 支持电信收短信！

只能收不能发；仅限中国电信，没有其他运营商适配；直接写到原机内查看短信的表里面，体感和移动/联通一样。

如果想玩改面板/独立短信转发面板的， `/tmp/zxct-ims.status` 可以查看电信 ims 状态。

**叠甲：只在 友猫/讯优/新讯4g棒子、格行快充充电宝、纽曼电池机 上测试通过；我手上没有其他的中兴微方案的机器了，不保证中兴微方案通杀**

感谢 damonto 的 [ims-go](https://github.com/damonto/ims-go)，本项目参考了其 ims 实现，也因此选择了 GPLv3 许可证。

警告：本项目大量使用 AI 生成代码，仅在部分实机上验证通过，且代码未经过完整人工 Review。自行承担使用风险。

下面是 AI 生成的 Readme。

---

在使用**中兴微（ZXIC）方案**的随身 Wi-Fi 设备内部，通过中国电信 IMS 接收短信，并将短信写入设备自带的短信数据库。固件中的 `zte_*` 名称是原厂程序或库的名称。

项目只实现中国电信 IMS 短信接收。它优先使用设备内部 AT 接口，必要时使用原厂 Web AT 接口；SIM 鉴权命令在结果不明确时不会跨接口重发。无线驻网和频段选择由调制解调器处理。

## 状态与限制

- 已在部分设备上验证 IMS 注册、重启自启动和普通短信接收；收到的短信可在原厂界面显示为未读。不同固件的兼容性仍需验证。
- 已在原厂空短信库设备上创建短信表并写入模拟短信，原厂界面显示正常。
- 短信按设备原有格式明文保存在设备数据库中。IMS 注册和报文完整性按运营商协商实现；当前仅支持 `null` 加密的 ESP，实际协商由网络决定。
- 程序仅在确认中国电信 SIM 后访问短信库和 IMS。若 SIM 在启动时尚未就绪，进程可能退出。

## 构建

获取源码时初始化 Mbed TLS 子模块：

```sh
git submodule update --init --recursive
```

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

从项目 Releases 下载适合电脑系统的部署包并解压。将插有中国电信卡的设备通过 USB 连接电脑，确保 ADB 已授权、设备有足够的可写空间。Windows 可能需要安装设备的 ADB 驱动；部署包已包含 ADB 程序。

Windows 双击 `install.cmd` 安装，双击 `uninstall.cmd` 卸载。Linux 部署包内的 ADB 适用于 x86-64 电脑；在解压后的目录打开终端，执行 `./install.sh` 安装，执行 `./uninstall.sh` 卸载。如果解压工具没有保留可执行权限，先执行 `chmod +x install.sh uninstall.sh platform-tools/adb`。

脚本在电脑上运行，通过 ADB 将所需文件传入设备并执行设备端安装或卸载脚本。安装程序保留已有的可写启动脚本，并在原厂短信开关为 `no` 时将其设为 `yes`。安装后需重启设备，重启会短暂断网；启动脚本会将压缩程序解压到内存中的 `/tmp/zxct-ims` 并启动。

卸载会移除程序和启动入口，并尝试停止正在运行的程序；如果设备不支持停止命令，重启后程序会停止。设备短信数据库和短信功能开关会保留。

从源码构建后，也可在仓库根目录使用这些脚本；此时需要在电脑 PATH 中准备 ADB。脚本使用 `build/zxct-ims.arm.gz` 作为程序文件。ADB shell 需要能够写入 `/mnt/userdata` 并执行设备上的 `nv` 命令。

首页的状态信息会显示用户态 IMS 状态。电信卡可使用“重试注册”按钮；操作时需要输入原厂后台密码，程序也会再次确认 SIM 属于中国电信。页面服务使用设备 `br0` 的局域网地址和 TCP 端口 `18765`；如果设备没有该接口，IMS 收信仍可运行，但页面状态和按钮不可用。连续输错密码五次后需等待五分钟。

后台状态位于 `/tmp/zxct-ims.status`：`registered` 为已注册，`starting` 和 `waiting` 表示尚未就绪。检查时也应确认进程存在。前台调试时，可先解压并运行程序，从标准输入输入原厂后台密码；设备管理终端可能回显输入，应使用可信终端。

## 许可证

项目代码采用 [GPL-3.0](LICENSE)，参考 ims-go 进行重写。

## 致谢

- [ims-go](https://github.com/damonto/ims-go)
- [alice-pusher-bot-zxic](https://github.com/Amamiyashi0n/alice-pusher-bot-zxic)
