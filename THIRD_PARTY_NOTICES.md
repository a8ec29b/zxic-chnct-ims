# 第三方组件与参考项目

| 项目 | 本项目中的用途 | 许可证及状态 |
| --- | --- | --- |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | 仓库包含 v3.6.8 中 MD5、SHA-1、HMAC、Base64 所需源码和头文件，构建时编入程序。 | Apache-2.0 或 GPL-2.0-or-later 双许可；本项目选择 Apache-2.0。原始版权和许可证保留在 [third_party/mbedtls](third_party/mbedtls/)。 |
| [SQLite](https://www.sqlite.org/copyright.html) | 主机构建使用 SQLite 开发库，设备运行时使用固件提供的 SQLite 接口。 | SQLite 上游将其代码贡献至公有领域。本仓库不分发 SQLite 或设备内的库；具体固件库的来源和许可由其提供方确定。 |
| [alice-pusher-bot-zxic](https://github.com/Amamiyashi0n/alice-pusher-bot-zxic) | 参考通过 `path_sh` 和原厂 `global.sh` 保持自启动的方案。 | Apache-2.0。 |
| [zxic-wifi-esim-manager](https://github.com/a8ec29b/zxic-wifi-esim-manager) | 参考用户提供的设备调查和 IMS 实验交接文档；构建不依赖该项目。 | MIT。 |
| [ims-go](https://github.com/damonto/ims-go) | 开发记录显示参考了短信、注册与鉴权源码；项目构建不包含或链接该 Go 库。 | GPL-3.0。逐项对照未找到能确认移植的证据，也不能证明独立创作；详见[来源核对](docs/ims-go-provenance.md)。 |
| [Howard Hinnant date](https://github.com/HowardHinnant/date/tree/v3.0.3) | `src/sms_pdu.c` 的日期换算与该项目算法高度相似；实际创作来源未能确认。 | MIT；为稳妥起见保留 [v3.0.3 版权与许可声明](third_party/howard-hinnant-date-LICENSE.txt)。 |
| [wwan-go](https://github.com/damonto/wwan-go)、[Sigmo](https://github.com/damonto/sigmo) | 先前设备调查中的关联项目，作为实验背景；本项目不包含或链接其代码。 | 两者均为 MIT。 |

构建使用的交叉工具链、uClibc、BusyBox 和目标固件中的厂商库由使用者自行提供，不包含在本仓库。其各自的许可证和相应的源码获取责任由分发者核实。

Mbed TLS 上游修订及文件范围记录在 [third_party/mbedtls/README.md](third_party/mbedtls/README.md)。本项目也参考相关 3GPP 规范，规范本身不是本仓库的源代码依赖。
