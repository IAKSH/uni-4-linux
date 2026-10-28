# uni-4-linux

Linux 控制工具，面向 **机械革命 极光Pro（MECHREVO Jiguang Pro，GM5AG0O / Uniwill ODM）**：
键盘 RGB 背光、触摸板开关（含 LED）与风扇状态/全速开关。

本项目是 `jiguang-pro-linux` 的 Modern C++ 重写版：

- 只保留 **acpi_call** 一条 EC 访问路径，移除 `/dev/port` 直写后端；
- 代码按职责平铺在 `src/`，没有额外的后端抽象层，KISS；
- 使用 C++23（`std::expected`、`std::format`、`std::span`）；
- 命令行解析用 [CLI11](https://github.com/CLIUtils/CLI11)，触摸板 HID 用
  [hidapi](https://github.com/libusb/hidapi)，测试用 [doctest](https://github.com/doctest/doctest)；
- 测试通过内存 EC 完成，不需要真机或 `acpi_call`。

## 功能

| 功能 | 说明 |
|---|---|
| `probe` | 读取 EC 型号、背光状态、触摸板状态 |
| `kbd color/brightness/power/effect/off` | 静态颜色、亮度、总电源、固件彩虹、熄灯 |
| `kbd breathe` | 软件呼吸灯（Ctrl-C 停止） |
| `tp get/on/off/toggle/watch` | 触摸板开关 + LED，Fn+F5 监听 |
| `fan get` / `fan boost on\|off` | 风扇模式/占空比/RPM，风扇全速 |
| `ec read/write/dump` | 原始 EC 寄存器读写/导出（调试） |
| `keymap` / `monitor` | AT 键盘扫描码映射、输入事件监视（调试） |

背光空闲自动熄灭由 **EC 固件**处理（约 10 分钟无输入后熄灭，按键即醒），
不需要常驻守护进程。

## 依赖

构建：

| 依赖 | Arch | Fedora |
|---|---|---|
| CMake >= 3.20 | `cmake` | `cmake` |
| C++23 编译器（GCC >= 13） | `gcc` | `gcc-c++` |
| CLI11（header-only） | `cli11` | `cli11-devel` |
| hidapi（hidraw 后端） | `hidapi` | `hidapi-devel` |
| doctest（仅测试） | `doctest` | `doctest-devel` |

运行：

- `acpi_call` 内核模块（Arch：`acpi_call-dkms`；其他发行版请从源码构建并加载）
- root 权限（写 `/proc/acpi/call` 与 hidraw feature report 需要）

本仓库开发环境缺少以上包时，安装命令示例：

```bash
# Arch（acpi_call-dkms 通常在 AUR，按你的内核选择安装方式）
sudo pacman -S --needed cmake gcc cli11 hidapi doctest

# Fedora（acpi_call 需自行构建）
sudo dnf install cmake gcc-c++ cli11-devel hidapi-devel doctest-devel
```

## 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

sudo ./build/uni4 probe
```

如果未安装 doctest，CMake 会跳过测试目标并给出提示。

## 安装

```bash
sudo scripts/install.sh
```

脚本安装二进制到 `/usr/local/bin/uni4`，并安装 systemd 单元模板，但**不启用**任何服务。
也可以只安装二进制：

```bash
sudo cmake --install build
```

## 用法

```bash
sudo uni4 probe

# 键盘背光
sudo uni4 kbd color 255 0 0          # 纯红（0-255，EC 会量化到 0-50）
sudo uni4 kbd color 0 255 0          # 纯绿
sudo uni4 kbd color 255 255 255      # 白色
sudo uni4 kbd brightness 3           # 亮度 0..4
sudo uni4 kbd power off              # 总电源关（彻底熄灭）
sudo uni4 kbd power on               # 总电源开
sudo uni4 kbd effect rainbow         # 固件彩虹动画
sudo uni4 kbd effect static          # 退出彩虹，回到静态色
sudo uni4 kbd breathe 255 0 0 2000   # 呼吸灯：颜色 + 周期 ms
sudo uni4 kbd off                    # 关闭背光

# 触摸板（开关 + LED）
sudo uni4 tp get                     # 1 = 启用，0 = 禁用
sudo uni4 tp toggle
sudo uni4 tp on
sudo uni4 tp off
sudo uni4 tp watch                   # 监听 Fn+F5 / KEY_F21 / 旧版 keymap 350

# 风扇
sudo uni4 fan get                    # mode / duty / RPM（只读）
sudo uni4 fan boost on               # CPU+GPU 风扇全速
sudo uni4 fan boost off              # 恢复自动

# 调试
sudo uni4 ec read 0x0769
sudo uni4 ec write 0x0769 50
sudo uni4 ec dump 0x0700 0x30        # 只读静态设置区，勿批量读电池区
sudo uni4 keymap f8 191              # 物理触摸板按键（可选，不可靠；旧版 350 也识别）
sudo uni4 monitor                    # 监听输入事件
```

`probe`/`ec read` 显示的是 EC 原始量化值（0-50）；`kbd color` 接受 0-255 并四舍五入到 0-50。

逆向辅助脚本 [scripts/colorsnap.sh](scripts/colorsnap.sh) 与
[scripts/snapshot.sh](scripts/snapshot.sh) 只是 `uni4 ec dump` 的封装，默认使用
`build/uni4`；已安装时可用 `UNI4=/usr/local/bin/uni4` 覆盖。

## systemd

```bash
# 开机彩虹 + 常亮
sudo systemctl enable --now uni4-kbd-rainbow

# 开机固定颜色：先编辑 RGB，再启用
sudo systemctl edit --full uni4-kbd-color
sudo systemctl enable --now uni4-kbd-color

# Fn+F5 触摸板切换
sudo systemctl enable --now uni4-tpwatch
```

颜色和效果都是一次性写入 EC，**持久保留**，不需要常驻进程。

## 设计

```
src/
  ec.hpp / ec.cpp          acpi_call 传输、EC 寄存器表、std::expected 错误
  hid.hpp / hid.cpp        hidapi 适配层：枚举/打开/feature report、HidBackend 概念
  kbd.hpp                 键盘背光（header-only 模板，便于用 FakeEc 测试）
  fan.hpp                 风扇读取与 boost
  touchpad.hpp / .cpp     feature report 7 协议，只依赖 hid.hpp
  input.hpp / .cpp        evdev 监听、Fn+F5、keymap、monitor
  util.hpp / .cpp         输出与 hexdump
  main.cpp                CLI11 命令树
tests/tests.cpp           内存 EC + HID 单元测试（无硬件）
```

EC 生产后端只有 `AcpiEc`；`Ec<Backend>` 是极薄的 read-modify-write 包装，
模板参数仅用于在测试中注入 `FakeEc`，不是运行时可切换的“模式”。

HID 生产后端只有 `HidApi`。`hid.hpp` **不包含** `<hidapi/hidapi.h>`，
hidapi 的类型、枚举和打开操作全部封在 `hid.cpp`；`touchpad` 通过
`HidBackend` 概念注入，测试用 `FakeHid` 直接驱动 feature report 逻辑。

输入监听直接使用 Linux evdev：`libevdev` 只封装事件解码，不负责设备枚举，
引入它并不会减少这里的代码量，因此没有增加依赖。

## 已知限制

- EC 只接受 0-50 颜色级别，颜色与呼吸灯有阶梯感，和 Windows 驱动一致。
- 背光约 10 分钟无输入自动熄灭是 EC 固件行为，软件无法禁用。
- 触摸板开关走触摸板控制器的 HID feature report 7；禁用时控制器自动点亮 LED。
- Fn+F5 会产生 `Meta+Ctrl+F24`，本工具据此切换；物理按键的 EC 扫描码注入不稳定，
  `keymap` 仅作为可选调试手段。
- 风扇 boost 只置 `0x0751` bit `0x40`，不写自定义风扇曲线（协议机型相关，写错有过热风险）。
- `ec dump` 请只读取静态设置区；批量读电池/温度实时区会与内核 ACPI EC 驱动竞争。

协议细节见 [docs/PROTOCOL.md](docs/PROTOCOL.md)，完整逆向过程见
[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md)。
