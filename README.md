<div align="center">
<img src="https://avatars.githubusercontent.com/u/245985800?s=200&v=4" style="width:100px;" width="100"/>
<h2>FasterEdge MCU - C51 (8051)</h2>
<h3>FasterEdge 框架的 8051 单片机平台实现（Keil C51 版）</h3>
</div>

### 一、简介

本项目是 **[FasterEdge](https://github.com/FasterEdge/FasterEdge)** 框架在 **8051（C51）** 单片机平台上的实现。8051 为 8 位内核、内置 RAM 仅 128~256 字节，无网络、无操作系统，因此在 [MCU-ESP32](../MCU-ESP32) / [MCU-ESP8266](../MCU-ESP8266) 基础上进一步精简能力子集。

- ✅ **仅 Keil C51 版**（8051 无 Arduino 版，Keil C51 是其标准工具链）
- ✅ 与主仓库**同名同命令**，云边协同对等编程
- ✅ HMAC-SHA256 纯 C 零依赖（8 位机可运行）
- ✅ 存储用 EEPROM 抽象（STC 内置 IAP / 外接 24C02）
- ✅ 全工程 C89 兼容写法，可在 Keil C51 直接编译

### 二、已实现能力（8051 合理子集）

**Ability（6 个）**

| 名称 | 类别 | 命令 |
|------|------|------|
| `BaseAbility` | 基础 | `list_data_names` / `list_ability_names` |
| `RoleAbility` | 角色 | `describe` / `set_role` / `get_role` |
| `TimeAbility` | 时间 | `sync_manual` / `sync_system` / `get_time` / `configure_run`（无 NTP）|
| `OneKeyAbility` | 令牌 | `issue_token` / `verify_token` / `revoke_all` / `list_tokens` / `status` / `rotate`（HMAC-SHA256）|
| `SerialAbility` | 串口 | `open` / `close` / `write` / `read` / `is_open` / `set_config` / `get_config` / `list_ports` |
| `ModbusAbility` | Modbus | `set_unit_id` / `get_unit_id` / `read_holding` / `read_input` / `read_coils` / `read_discrete` / `write_holding` / `write_coil`（RTU 从站）|

**Data（2 个）**

| 名称 | 功能 | 命令 |
|------|------|------|
| `BaseData` | 框架元信息 | `logo` / `info` |
| `ConfigData` | KV 配置（EEPROM 持久化）| `get` / `set` / `delete` / `list` / `snapshot` |

### 三、排除项与理由

| 能力 | 排除原因 |
|------|---------|
| MQTTAbility / NetMapData | 8051 无网络协议栈 |
| EdgeRoleAbility | 依赖网络心跳上报 |
| ConfigFileAbility | 与 ConfigData 重复，且无文件系统概念 |
| KeyringData | 与 OneKeyAbility 合并（同一 EEPROM 密钥存储）|
| TimeAbility.sync_ntp | 无网络无法 SNTP 校时 |

### 四、目录结构

```
MCU-C51/
├── keil/                       # Keil C51 版（uVision 工程）
│   ├── MDK-ARM/                # FasterEdge-MCU-C51.uvproj（Keil C51 工程）
│   ├── Core/                   # fe.h / fe.c / fe_hmac_sha256.c（纯 C）
│   ├── Inc/                    # fe_ability.h / fe_data.h / fe_port.h
│   ├── Ability/                # ability_*.c（6 个）
│   ├── Data/                   # data_*.c（2 个）
│   └── User/                   # main.c / register.c / fe_port.c（移植层）
└── platformio_ide/             # VS Code + PlatformIO 插件工程（STC 平台 + SDCC）
    ├── platformio.ini          # stc / stc89c52rc（或 stc15f2k60s2）
    ├── .vscode/extensions.json # 推荐 PlatformIO IDE 插件
    ├── include/                # fe.h / fe_ability.h / fe_data.h / fe_port.h / fe_hmac_sha256.h
    └── src/                    # 复用 keil 裸机 C + SDCC 版 fe_port（8051 寄存器级实现）
```

> C51 无 Arduino 版；两套裸机 C 工具链：`keil/`（Keil C51）与 `platformio_ide/`（SDCC，VS Code 插件），能力与命令完全一致。

### 五、使用说明

1. 用 Keil C51（uVision）打开 `keil/MDK-ARM/FasterEdge-MCU-C51.uvproj`
2. 在 `User/fe_port.c` 中完成平台移植：
   - **UART**：初始化定时器 1 波特率 + 串口中断（TODO 注释附代码骨架）
   - **EEPROM**：文件末尾附 **STC89/STC15 内置 IAP** 与 **AT89S52 + 24C02 I2C** 两套参考实现
   - **时间**：定时器 0 秒中断计数
3. 编译生成 HEX，烧录到 8051
4. 通过串口（默认 115200）输入命令

**串口命令示例：**

```
help
ability_BaseAbility list_ability_names
ability_RoleAbility set_role edge
ability_TimeAbility sync_manual 1700000000
ability_OneKeyAbility issue_token sensor01
ability_ModbusAbility set_unit_id 3
ability_ModbusAbility write_holding 0,42
ability_ModbusAbility read_holding 0,4
ability_SerialAbility set_config 0,9600
ability_SerialAbility write hello
data_ConfigData set wifi.ssid=MyNet
data_ConfigData get wifi.ssid
data_BaseData info
```

### 六、C51 适配要点

| 差异点 | ESP32/ESP8266 | C51 (8051) |
|--------|--------------|-----------|
| 编译器 | PlatformIO (GCC) / Keil MDK | **Keil C51** |
| 类型 | `uint32_t` / `size_t` | `u8` / `u16` / `u32` / `u64`（`long long`）|
| int 位宽 | 32 位 | **16 位**（代码避免依赖 int 位宽）|
| 存储 | NVS / Flash | EEPROM（STC IAP 或 24C02）|
| 网络 | 有 | **无**（能力子集剔除网络项）|

### 六-b、PlatformIO IDE 版使用（VS Code 插件）

`platformio_ide/` 是 **SDCC 编译器版** 工程（PlatformIO STC 平台），复用 keil 版 C 代码，`fe_port.c` 为 SDCC 兼容的 8051 寄存器级实现（UART 轮询收发 / 波特率定时器 / TODO 留 EEPROM 与时间）。无需 Keil 即可在 VS Code 中编译烧录。

1. VS Code 安装 **PlatformIO IDE** 插件（打开 `platformio_ide/` 时自动提示）
2. 打开 `platformio_ide/` 目录
3. 底部状态栏点击 **Build** / **Upload** / **Serial Monitor**（115200）

```bash
cd platformio_ide
pio run            # 编译（生成 HEX）
pio run -t upload  # 烧录
pio device monitor # 串口监视
```

> 换芯片：编辑 `platformio.ini` 将 `board` 改为 `stc15f2k60s2` 等其他 STC 板型；需要 `%lu` 长格式化时放开 `-Dprintf=printf_large`。串口命令与 keil 版完全一致。
| 内存 | KB~MB | **128~256B**（缓冲缩至 96B，大表放 xdata）|
| C99 特性 | 可用 | **C89 兼容**（无复合字面量，静态模块表）|

### 七、与 FasterEdge 主仓库的对应关系

- 命令名与主仓库**完全一致**，与 MCU-ESP32/ESP8266 实现同构
- `Atom` 模型：单例全局 Atom，`data_` / `ability_` 前缀路由
- 令牌用 HMAC-SHA256（纯 C，无 mbedTLS），密钥 EEPROM 持久化
- Modbus 寄存器表存 RAM，RTU 帧服务入口 `modbus_slave_service()` 已预留

### 八、姊妹项目

- **[FasterEdge MCU - ESP32](https://github.com/FasterEdge/MCU-ESP32)**：9 Ability + 4 Data
- **[FasterEdge MCU - ESP8266](https://github.com/FasterEdge/MCU-ESP8266)**：9 Ability + 4 Data
- **[FasterEdge](https://github.com/FasterEdge/FasterEdge)**：框架主仓库
