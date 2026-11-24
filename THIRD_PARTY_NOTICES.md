# 第三方组件与参考项目

| 项目 | 本项目中的用途 | 许可证及状态 |
| --- | --- | --- |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | 仓库包含 v3.6.8 中 MD5、SHA-1、HMAC、Base64 所需源码和头文件，构建时编入程序。 | Apache-2.0 或 GPL-2.0-or-later 双许可；本项目选择 Apache-2.0。原始版权和许可证保留在 [third_party/mbedtls](third_party/mbedtls/)。 |
| [SQLite](https://www.sqlite.org/copyright.html) | 主机构建使用 SQLite 开发库，设备运行时使用固件提供的 SQLite 接口。 | SQLite 上游将其代码贡献至公有领域。本仓库不分发 SQLite 或设备内的库；具体固件库的来源和许可由其提供方确定。 |
| [alice-pusher-bot-zxic](https://github.com/Amamiyashi0n/alice-pusher-bot-zxic) | 参考通过 `path_sh` 和原厂 `global.sh` 保持自启动的方案。 | Apache-2.0。 |
| [ims-go](https://github.com/damonto/ims-go) | 参考 IMS 注册与短信处理流程；项目构建不包含或链接该 Go 库。 | GPL-3.0。代码来源和是否构成改写仍需在公开发布前确认，不能仅凭语言不同认定许可兼容。 |

构建使用的交叉工具链、uClibc、BusyBox 和目标固件中的厂商库由使用者自行提供，不包含在本仓库。其各自的许可证和相应的源码获取责任由分发者核实。

Mbed TLS 上游修订及文件范围记录在 [third_party/mbedtls/README.md](third_party/mbedtls/README.md)。本项目也参考相关 3GPP 规范，规范本身不是本仓库的源代码依赖。
