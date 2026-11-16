# 备份存档与保留数据升级

游戏进度与亮度、音量、息屏时间都保存在工牌的 NVS 分区。v0.6.1 可读取 v1 至 v5 的旧进度。第一次进入新版时，如果还没有钓鱼游戏自己的设置，会读取兼容的旧工牌设置。

## 备份

关掉占用串口的监视器，接上工牌。在有 esptool 的 ESP-IDF 环境下，先确认是目标工牌，替换命令中的 COM11。以下文件名请按日期另存，不要覆盖已经验证好的备份。

```powershell
python -m serial.tools.list_ports -v
python -m esptool --chip esp32c3 --port COM11 read-flash 0x0 0x800000 passport-before.bin
python -m esptool --chip esp32c3 --port COM11 verify-flash 0x0 passport-before.bin
Get-FileHash passport-before.bin -Algorithm SHA256
```

以上完整备份适用于 8 MB 版工牌，包含固件、分区表、存档和设备配置。文件和校验值请保存在仓库外的私有位置。只备份存档时，应先从设备实际分区表确认 NVS 的起始地址和大小。对应本项目分区表的设备是 `0x9000 / 0x6000`：

```powershell
python -m esptool --chip esp32c3 --port COM11 read-flash 0x9000 0x6000 passport-nvs.bin
python -m esptool --chip esp32c3 --port COM11 verify-flash 0x9000 passport-nvs.bin
```

## 升级时保留存档

先核对设备芯片、容量、实际分区表和应用镜像的 SHA256。当前项目分区表的应用从 `0x10000` 开始，容量 `0x3f0000`。只有实际布局匹配、镜像及其擦除范围能完全放入应用分区时，才从 `0x10000` 写入应用文件 `nabo-fishing.bin`。写完后回读验证应用和 NVS。可使用本地《NABO 冒险之旅》工程的 `firmware/tools/flash_candidate.py --mode upgrade`，它会先读设备分区表、核对 MAC 和镜像哈希、备份并校验，再刷应用和回读保护区。新版不能装入旧版约 2 MB 的应用分区。

全量合并镜像用于从 `0x0` 安装，会覆盖 NVS 所在区域。如果必须全量安装且需要恢复数据，应先保留完整备份，再按同一块设备、相容的 NVS 布局核对后单独恢复备份的 NVS。这个操作会同时还原该分区里其他应用的设置和记录。不要把应用镜像写到 `0x0`，也不要把合并镜像写到 `0x10000`。
