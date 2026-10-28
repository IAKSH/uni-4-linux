# GM5AG0O / Uniwill EC 协议

本文只记录 `uni-4-linux` 实际使用的接口。完整探索过程见
[../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md)。

## EC 访问

通过 `acpi_call` 调用 ACPI 方法：

```
\_SB.INOU.ECRR 0x<addr>          # 读
\_SB.INOU.ECRW 0x<addr> 0x<val>  # 写
```

`/proc/acpi/call` 写入命令后，EC 方法大约需要几毫秒完成。本项目每次调用后等待
6 ms，再读取结果文本（`0x32` 等）并解析。不要用 `/dev/port` 直写：它无法可靠写入
本机 EC，还会与内核 ACPI EC 驱动抢端口。

## 键盘 RGB

| 地址 | 含义 | 说明 |
|---|---|---|
| `0x0769` | R | 0..50 |
| `0x076A` | G | 0..50 |
| `0x076B` | B | 0..50 |
| `0x0767` | 提交/效果 | bit `0x20` = 提交颜色（EC 应用后自清）；bit `0x80` = 固件彩虹 |
| `0x078C` | 亮度/电源 | bit `0x10` = 应用亮度；bit `0x02` = 总电源关；bit 5..7 = 亮度 0..4 |
| `0x07C5` | 电源准备 | 开关电源前重写原值 |
| `0x0740` | project id | 探测 |
| `0x073C` | keyboard id | 探测 |
| `0x0766` | 支持位 | bit `0x04` 提示单区 RGB |

0..255 输入换算：

```
level = clamp((value + 4) / 5, 0, 50)
```

设置静态颜色：

1. 写 `0x0769/0x076A/0x076B`；
2. 读 `0x0767`，清 bit `0x80`、置 bit `0x20`，写回。

呼吸灯每帧重复上述写+提交。固件彩虹只需置 `0x0767` bit `0x80`。

电源：

- 关：重写 `0x0767`，重写 `0x07C5`，置 `0x078C` bit `0x02`。
- 开：清 `0x078C` bit `0x02`，重写 `0x0767`/`0x07C5`，读回 RGB 再写回，
  最后提交 `0x0767` bit `0x20`。

## 风扇

| 地址 | 含义 |
|---|---|
| `0x0751` | 模式；bit `0x40` = boost，bit `0x80` = manual |
| `0x075B` / `0x075C` | 左/右占空比（原始值 = 百分比 × 2） |
| `0x0464`/`0x0465` | 左 RPM 高/低字节 |
| `0x046C`/`0x046B` | 右 RPM 高/低字节 |

boost 使用读-改-写只操作 bit `0x40`，保留其它位。

## 触摸板

触摸板是 PixArt/Uniwill I2C-HID 设备（`UNIW0001`，vendor `093A`，product `0255`）。
EC 寄存器不控制它，使用标准 HID feature report：

```
Report ID 7
Byte 0: report ID
Byte 1: Touchpad Configuration
        bit 0 = 1 启用 / 0 禁用
```

禁用时触摸板控制器自动点亮 LED。通过 hidapi 的
`hid_get_feature_report` / `hid_send_feature_report` 读写，长度 2 字节。

实现上，[touchpad.hpp](../lib/include/uni/touchpad.hpp) 的协议逻辑只依赖 `HidBackend` 概念；
[hid.cpp](../lib/src/hid.cpp) 负责 hidapi 枚举、打开和 feature report I/O。

## Fn+F5

Fn+F5 在 Linux 下产生：

```
KEY_LEFTMETA + KEY_LEFTCTRL + KEY_F24 (194)
```

`tp watch` 监听所有 `/dev/input/event*`，在 F24 按下且 Meta/Ctrl 同时按住时切换。
也接受 `KEY_F21`、`KEY_TOUCHPAD_TOGGLE`，以及旧映射使用的 keycode 350。

## 注意

- 颜色/亮度寄存器是 EC 的暂存区，必须通过 `0x0767` 提交位生效。
- 不要批量读取电池/温度实时区，会干扰内核 ACPI EC 驱动。
- 背光空闲熄灭由 EC 固件控制，没有可写的超时寄存器。
