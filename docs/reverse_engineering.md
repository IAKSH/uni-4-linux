# 逆向过程记录：如何在缺少资料的情况下调通 GM5AG0O 的键盘 RGB 与触摸板

本文档完整记录 `uni` 工具的开发过程——在没有任何厂商文档的情况下，如何从零开始
逐步试探、验证、最终调通 MECHREVO 极光Pro (GM5AG0O) 的键盘 RGB 背光和触摸板开关。
记录每一轮假设、实验方法、失败原因与最终结论，供后续维护和同类机型参考。

---

## 1. 背景与目标

- **设备**: MECHREVO Jiguang Pro Series GM5AG0O（机械革命极光Pro）
- **平台**: Arch Linux, Hyprland (Wayland), Kernel 7.1.9
- **目标**:
  1. 键盘 RGB LED：控制颜色、常亮/呼吸、无输入定时熄灭
  2. 触摸板开关：左上角触摸按钮（带 LED 指示灯，禁用时亮）

要求纯用户态实现。

---

## 2. 硬件识别（第 1 步：确认这是什么机器）

用 fastfetch / DMI 信息确认平台：

```
DMI:  product_name = Jiguang Pro Series GM5AG0O
      sys_vendor  = MECHREVO
      board_name  = GM5AG0O
      bios        = N.1.13MRO16
      EC firmware = 1.15
CPU:  12th Gen Intel Core i7-12700H (Alder Lake)
```

关键观察：
- `GM5AG0O` 是 **Tongfang 公模命名**（GM5xxx 系列），但后续发现其 EC 实为 **Uniwill 设计**
- 触摸板 ACPI HID 为 `UNIW0001`，厂商 093A（PixArt），走 I2C
- ACPI 设备列表里有 `INOU0000:00`（后来证明这是控制 EC 的入口）
- EC 设备 `PNP0C09` 有两个：`\_SB.PC00.LPCB.H_EC`（空壳）和 `\_SB.PC00.LPCB.EC0_`（真 EC）
- WMI GUID `ABBC0F6A..72` 齐全 → 符合 **Uniwill** 的 WMI 特征

**经验**：同一模具可能被多个品牌贴牌，DMI 名不可靠；要认 ACPI HID、WMI GUID、
触摸板 HID 这些“指纹”。

---

## 3. 第一轮假设：tuxedo-drivers 的 Uniwill 寄存器映射

### 依据
- Arch 上有 `mechrevo-drivers-dkms`（tuxedo-drivers 的移植），社区确认机械革命
  Uniwill 模具可用
- tuxedo-drivers 的 `uniwill_keyboard.h` 定义：
  - 颜色 `0x0769/0x076A/0x076B`（R/G/B，0-50）
  - 提交位 `0x0767 |= 0x20`
  - 亮度/状态 `0x078C`

### 实现
用 `/dev/port` + `iopl(3)` 直读 EC。先按 DSDT 反汇编出的 Uniwill 邮箱协议实现
16 位地址读写（LDAT/HDAT/FLAGS/DRDY/CMDL/CMDH 位于 EC RAM 0x8A-0x8F）：

```
读: 置 BFLG → 写 LDAT/HDAT(地址) → 置 RFLG → 等 DRDY → 读 CMDL
写: 置 BFLG → 写 LDAT/HDAT(地址) → 写 CMDL/CMDH(数据) → 置 WFLG → 等 DRDY
```

### 结果
- ✅ **读**完全正常：barebone ID `0x82`、RGB 寄存器读出 `0x32,0x32,0x00`
- ❌ **写**不生效：写入后读回原值

**结论**：读能用、写不能 → 直接端口访问不适用于本机 EC 的写入路径。

---

## 4. 第二轮：颜色“能变又不能变”的困惑

在 `/dev/port` 路径下尝试 `kbd color 0 255 0`，颜色偶尔会变（绿），但多数时候无响应，
且不可复现。期间观察到几个重要现象：

- 按 **Fn+F6/F7** 亮度键始终有效（EC 内部处理）
- 写颜色后按 **Fn+F6**，有时会触发颜色应用
- **休眠/唤醒后**颜色会变化（EC 重新应用了保存的状态）

用“EC 快照对比法”抓取前后寄存器差异：反复测试后 0x0769-0x76B 始终为
`0x32,0x32,0x00`，无论我写什么。**这说明两个问题**：

1. 写入值域不对：我按 0-255 写入 `0xFF`，而 EC 只接受 **0-50**，越界值被拒绝
2. 写入路径不对：`/dev/port` 的邮箱写未能真正落到 EC

**转折点**：某次休眠后键盘由绿变黄，快照显示 0x0769-0x76B 变成 `0x94,0x94,0x00`
（0-50 值域解释不通，0-255 解释为 148,148,0 = 黄色，与视觉一致）→ 进一步证明这些
寄存器就是颜色，只是值域与写入路径两件事都还没对。

---

## 5. 事故：全量 dump 把电池读数打乱了（重要教训）

为了找颜色寄存器，写了个“全量 dump EC RAM”命令，结果：

- 电池监测出现疯狂跳动的乱码数值
- 立即终止并重启后恢复正常，无硬件损伤

**原因**：EC RAM 低位区域（0xC0-0xFF）就是电池/ACPI 实时数据区。内核的 ACPI EC 驱动
周期性通过同一对端口 (0x62/0x66) 读电池。我的直接端口访问与其**并发竞争**，导致内核
读到错位的数据。

**教训**：
- 用户态直接端口访问 EC 会与内核 ACPI EC 驱动抢端口，绝不能做大流量操作
- 批量读取实时数据区（电池/温度）尤其危险
- 正确做法是用内核同步的访问路径（ACPI 方法 / acpi_call / 内核模块）

---

## 6. 关键突破：Mechrevo-RGB-Handoff 逆向项目

网上找到 `PPKunOfficial/Mechrevo-RGB-Handoff`——对另一台机械革命（蛟龙 X6DR55）的
控制中心做逆向，交付出完整协议。要点：

- 访问入口：ACPI 设备 `\_SB.INOU` 的 `ECRR(addr)` / `ECRW(addr, value)` 方法
- EC 是一个**系统内存窗口**：`0xFE410000 + addr`（EC 的 16 位地址空间）
- 键盘协议与 tuxedo-drivers 完全一致：
  - 颜色 0x0769/0x76A/0x76B，值域 0-50
  - 提交 `0x0767 |= 0x20`（EC 应用后自清）
  - 亮度 `0x078C = (旧&0x0F) | (level<<5) | 0x10`
  - 总电源 `0x078C bit0x02`，开关有固定序列（重写 0x0767/0x07C5）
- 触摸板：Windows 控制中心有 `TouchpadToggle_ON/OFF` 方法，写 `0x7A6`
  （bit6 触摸板禁用 + bit3 LED）

### 为什么 acpi_call 就成功了？
`acpi_call` 模块调用 `\_SB.INOU.ECRW`，由内核执行，与 ACPI EC 驱动正确同步、串行化。
改用 acpi_call 后：

- `kbd color 255 0 0` → **立即纯红**，颜色/亮度/电源全部正常
- 证实此前 `/dev/port` 写不进去的问题，是端口竞争/时序所致

**结论**：本机 EC 写入必须走内核同步路径（acpi_call 或内核模块）。

---

## 7. 触摸板：EC 寄存器全部失效 → 转向 HID

按蛟龙的逆向结果，先试 EC 寄存器：

| 目标 | 实验 | 结果 |
|---|---|---|
| `0x7F3` bit0 / 全值 0x00/0x01/0x02 | 写后观察触摸板 | ❌ 无效果 |
| `0x7A6` bit6 (0x40) | 置位后观察 | ❌ 无效果 |
| `0x7A6` bit3 (LED) | 置位后观察 LED | ❌ LED 不亮 |

同时确认了写入本身是有效的（`0x7A6` 写 `0x49` 读回 `0x49`）→ 不是写不进去，
**而是这些寄存器在本机根本不管触摸板**。

### 关键实验数据
- 物理按钮（触摸板左上角）按下去：monitor 只看到 BTN_TOUCH，无任何 EC/系统行为
- **Fn+F5**（Windows 上的触摸板开关键）→ 产生 **Super+Ctrl+Zenkaku/Hankaku**
  （KEY_125 + KEY_29 + KEY_194）——正是 tuxedo-drivers 用 i8042 filter 拦截的
  Uniwill 触摸板切换序列。但 Linux 下无人处理，不生效
- 全 EC 快照 diff：按按钮前后无任何触摸板相关寄存器变化

**结论**：这台机器的触摸板开关是**触摸板控制器（PixArt）自己处理**的，不走 EC。

### 突破口：读 HID report descriptor

`/sys/bus/hid/devices/0018:093A:0255.0003/report_descriptor` 里找到标准
**"Touchpad Configuration" feature report**：

```
Usage page 0x0D (Digitizers)
Usage 0x22 (Finger)
Collection Application
  Usage 0x57 (Touchpad Configuration)    ← 触摸板开关
  Usage 0x58 (Switch)                     ← 开关状态
  Logical Min 0, Max 1, Size 1, Count 2
  Report ID 7
  Feature (0xB1)
```

即 Windows Precision Touchpad 标准协议里的触摸板开关报告。

### 验证
通过 `/dev/hidraw`（触摸板是 hidraw2）用 `HIDIOCSFEATURE` 发送 report 7：

- `tp toggle` → 触摸板立即禁用，**LED 亮起** ✅
- `tp toggle` → 触摸板恢复，**LED 熄灭** ✅

**结论**：触摸板开关 + LED 由固件通过 HID feature report 7 控制（bit0=1 启用 / =0 禁用），
禁用时控制器自动点亮 LED。

### 物理按键（LED 下方的独立按键）—— 已判死路

Windows 上这个物理按键开关触摸板。调查过程：

1. **无 evdev 事件**：按按键（启用/禁用触摸板时）都不产生任何输入事件
2. **dmesg 暴露真相**：偶尔产生 `atkbd: Unknown key (translated set 2, code 0xf8)`，
   即 EC 把扫描码 **0xf8 (e0 78)** 注入到 PS/2 键盘控制器 (i8042) —— 走键盘路径，
   不走 EC 寄存器，也不走触摸板 HID
3. **扫描码注入不稳定**：只在某段时间偶然出现过，之后完全不产生；Windows 上同样
   难触发（需要多次按压）。这是固件/硬件行为，用户态无法修复
4. **映射尝试**：`setkeycodes e078` / `EVIOCSKEYCODE_V2` 映射 e078 报 EINVAL，
   `f8` 映射成功但收不到按键（因为扫描码根本没再产生）

**结论**：物理按键因固件注入不稳定而不可用，**改用 Fn+F5 作为触摸板切换热键**
（Fn+F5 稳定产生 Super+Ctrl+F24，可在 `tp watch` 中可靠检测）。

---

## 8. 呼吸灯：看似简单的一处坑

- 最初实现：循环改 0x0769-0x76B → **不生效**
- 原因：只写了颜色寄存器，**没有提交**（`0x0767 |= 0x20`）
- 修复：每帧写完 RGB 后读取 0x0767 并置位 0x20 → 正常呼吸

**经验**：这个 EC 的“颜色寄存器”其实是**暂存区**，必须靠 0x0767 的提交位才落地。
这也解释了早期“写颜色后要按 Fn+F6 才会变色”——Fn 键恰好触发了 EC 的应用逻辑。

---

## 9. 无输入自动熄灭：EC 固件自带，无需软件

最初以为需要软件守护进程，但实测发现**本机 EC 固件自带约 10 分钟的空闲熄灯**：

- 不运行任何软件时，背光在约 10 分钟无输入后自动熄灭，按键/触摸即醒
- 熄灭时 EC 状态（0x078C 电源/亮度、0x0769-0x76B 颜色）**仍显示"开"**，
  说明 EC 直接关断 LED 输出，未经过可读的 EC RAM 寄存器
- 灯亮/灯灭对比（覆盖全部可读区域 + 低区 0x0000-0x00FF）：唯一稳定变化是
  0x0741 位 0x04（状态位，只读，写入会被 EC 立即改回）
- 尝试无界14 的背光时间寄存器 0x34（0=30s/1=1min/-1=永不）：本机写 0xFF 无效，
  说明编码/寄存器机型相关
- 逆向 JIAOLONG 控制中心发现其 `KeyboardCloseTimer` 是**纯软件计时**
  （挂钩系统原始输入 + 定时器 + 命令 EC 关灯），不写 EC 寄存器

**结论**：本机"无输入自动熄灯"由 EC 固件实现（约 10 分钟，不可配置/禁用），
`uni` 无需也**不再提供**软件 idle daemon。若想要更短超时或彻底常亮，
需内核模块（i8042 filter 定期注入扫描码重置 EC 计时）或抓 Windows 控制中心的
实际 EC 写入来定位配置寄存器。

---

## 10. 最终结论

### 协议总表（GM5AG0O / Uniwill EC）

| 功能 | 地址/接口 | 说明 |
|---|---|---|
| EC 访问 | `\_SB.INOU.ECRR/ECRW`（内存窗口 0xFE410000） | 必须走 acpi_call/内核，端口直写不可靠 |
| 颜色 R/G/B | `0x0769 / 0x076A / 0x076B` | 值域 **0-50**（0-255→`clamp(ceil(v/5),0,50)`） |
| 颜色提交 | `0x0767 \|= 0x20` | EC 应用后自动清 0 |
| 亮度 | `0x078C = (旧&0x0F)\|(level<<5)\|0x10` | level 0..4 |
| 键盘总电源 | `0x078C bit0x02`（1=关） | 关闭需重写 0x0767/0x07C5；开启重写 RGB 后提交 |
| 触摸板开关+LED | HID feature report ID 7（PixArt） | bit0=1 启用/=0 禁用，禁用时固件亮 LED |

### 方法论总结（缺资料时的探索路径）

1. **确认硬件指纹**：DMI + ACPI HID + WMI GUID + 触摸板 HID，不要信品牌名
2. **找同类项目的逆向成果**：tuxedo-drivers / Mechrevo-RGB-Handoff 大大缩短进程
3. **先读后写、小步验证**：每个寄存器先读基线，再写入，立即读回确认生效
4. **用受控实验代替猜测**：快照对比（改颜色前后 diff EC）、Fn 键触发、休眠触发
5. **区分“写不进去”和“寄存器不对”**：写入后读回验证，再判断是否有效果
6. **不要大流量直读 EC**：会与内核 ACPI EC 驱动抢端口，污染电池/温度读数
7. **值域/提交机制是隐藏的坑**：0-50 值域、0x0767 提交位、0x078C apply 位，
   缺一不可，这就是“写入看起来失败”的常见原因

### 遗留事项

- 触摸板切换热键用 **Fn+F5**（可靠）。物理按键因 EC 扫描码注入不稳定而放弃
  （详见上文"物理按键"一节）
- 键盘灯光色偏/呼吸阶梯感为 EC 0-50 级量化导致，与 Windows 驱动一致，非 bug
