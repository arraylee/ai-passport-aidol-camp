# 京东 Aidol 创造营 · 碰友对讲

明星&博主同款玩法：多人对讲、碰一碰交友、JOY 唱跳挑战。

![京东 Aidol 创造营：明星&博主同款玩法](docs/images/cover.png)

固件 **v0.7.11**，配套蓝牙助手 **v0.3.6**。封面为 AI 生成的玩法示意图。

同伴可以离线组队，对全体或指定队友按住说话；靠近并双方确认后交换名片。唱跳挑战用声音控制 JOY：发声前进、升调跳高、停声换气，到终点后比较房间成绩。

## 下载与上手

在 [Releases](https://github.com/arraylee/ai-passport-aidol-camp/releases/tag/v0.7.11) 下载完整网页刷机 BIN、分段固件包和蓝牙助手。社区页面提供同一份 BIN。

- [中文快速上手](docs/QUICKSTART.zh.md)
- [English quick start](docs/QUICKSTART.en.md)
- [蓝牙助手说明](tools/device_console/README.md)：Mac 扫描多台设备，分别选择人物，设置并重置或独立清空数据；完成后自动断开连接，无需 USB 和重启。

官网刷机时选 `AI-PASSPORT-BLE-Config-v0.7.11-web.bin`，起始地址 **0x0**，不要全片擦除。网页合并 BIN 保留原 SN 身份区，但会覆盖普通 NVS；需保留普通 NVS 时使用分段包和 `tools/safe_flash.py`。分段应用 BIN 的地址为 **0x10000**。

完整网页 BIN SHA-256：

```text
f5627fdb8e979212f404cc7c08231f783cc6e504de1aa21c80281d0319a384d2
```

## 版本与预设

v0.7.11 将首页昵称区调整到 108px，供“京东3C数码”以原 20px 字体完整静止一行显示。其他长名字保留原滚动规则。包含 v0.7.10 的好友页主动寻找，以及此前的对讲恢复、游戏结算和排行榜修复。

蓝牙助手 v0.3.6 的第 6 个预设为“京东3C数码”，使用单只站立 JOY 的透明头像，签名为“谁敢横刀立马 唯我3C数码 战斗战斗 只做第一”。通过蓝牙应用到设备；刷入固件不会自动将每台设备改成该人物。内置演示资料和助手人物预设是独立内容。

## 构建与测试

使用 ESP-IDF **5.5.3**。依赖按 `dependencies.lock` 安装，不需要云服务密钥。

```sh
. /path/to/esp-idf-v5.5.3/export.sh
idf.py build size
python3 -m pip install Pillow
cmake -S tests -B build-host
cmake --build build-host -j6
ctest --test-dir build-host --output-on-failure
python3 tests/test_release.py
python3 tests/test_safe_flash.py
python3 tools/package_ble_release.py
python3 tools/verify_release.py releases/v0.7.11
```

蓝牙助手：

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/device_console/requirements.txt
.venv/bin/python -m unittest discover -s tools/device_console/tests
```

## 验证范围

发布的是此前交付的 v0.7.11 固件，BIN 字节与原交付一致。本次整理公开源码、上手说明、封面和助手 v0.3.6，没有改动固件运行逻辑。发布包包含构建、宿主测试、哈希和写入地址证据。

该版本尚未完成整机、多机射频、音频、20 分钟睡眠唤醒和完整充放电验收；宿主测试与构建通过不代表上述实测通过。空闲休眠设置为 20 分钟；游戏、通话、蓝牙管理或待处理工作会推迟休眠。不提供未经实测的续航承诺。

## 源码与素材

原创程序代码使用 [MIT License](LICENSE)。字体、第三方代码、人物照片、JOY、京东/Aidol 标识、活动 KV 与音频分别保留原权利和许可，不属于 MIT 授权，见 [素材说明](THIRD_PARTY_NOTICES.md)。

[开发历史与技术说明](docs/DEVELOPMENT-HISTORY.md) · [手动加好友](docs/MANUAL-FRIENDS.md) · [蓝牙接口](docs/BLE-CONSOLE.md)
