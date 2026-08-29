<div align="center">
<img src="https://avatars.githubusercontent.com/u/245985800?s=200&v=4" style="width:100px;" width="100"/>
<h2>FasterEdge MCU - C51 (8051)</h2>
<h3>8051 MCU implementation of the FasterEdge framework (Keil C51)</h3>
</div>

### 1. Introduction

This project is the **8051 (C51)** MCU implementation of the **[FasterEdge](https://github.com/FasterEdge/FasterEdge)** framework. The 8051 is an 8-bit core with only 128-256 bytes of internal RAM, no network, and no OS, so the capability subset is further trimmed down from [MCU-ESP32](../MCU-ESP32) / [MCU-ESP8266](../MCU-ESP8266).

- ✅ **Keil C51 version only** (8051 has no Arduino port; Keil C51 is its standard toolchain)
- ✅ **Same names and commands** as the main repo
- ✅ Pure-C HMAC-SHA256, zero dependencies (runs on 8-bit MCU)
- ✅ Storage abstracted over EEPROM (STC built-in IAP / external 24C02)
- ✅ C89-compatible code, builds directly with Keil C51

### 2. Implemented Capabilities (8051-reasonable subset)

**Ability (6)**

| Name | Category | Commands |
|------|----------|----------|
| `BaseAbility` | Base | `list_data_names` / `list_ability_names` |
| `RoleAbility` | Role | `describe` / `set_role` / `get_role` |
| `TimeAbility` | Time | `sync_manual` / `sync_system` / `get_time` / `configure_run` (no NTP) |
| `OneKeyAbility` | Token | `issue_token` / `verify_token` / `revoke_all` / `list_tokens` / `status` / `rotate` (HMAC-SHA256) |
| `SerialAbility` | UART | `open` / `close` / `write` / `read` / `is_open` / `set_config` / `get_config` / `list_ports` |
| `ModbusAbility` | Modbus | `set_unit_id` / `get_unit_id` / `read_holding` / `read_input` / `read_coils` / `read_discrete` / `write_holding` / `write_coil` (RTU slave) |

**Data (2)**

| Name | Function | Commands |
|------|----------|----------|
| `BaseData` | Framework metadata | `logo` / `info` |
| `ConfigData` | KV config (EEPROM) | `get` / `set` / `delete` / `list` / `snapshot` |

### 3. Excluded Capabilities & Rationale

| Capability | Reason for exclusion |
|------------|----------------------|
| MQTTAbility / NetMapData | No network stack on 8051 |
| EdgeRoleAbility | Depends on network heartbeat reporting |
| ConfigFileAbility | Duplicates ConfigData; no filesystem concept |
| KeyringData | Merged into OneKeyAbility (same EEPROM secret storage) |
| TimeAbility.sync_ntp | No network for SNTP |

### 4. Directory Layout

```
MCU-C51/
├── keil/                       # Keil C51 version (uVision project)
│   ├── MDK-ARM/                # FasterEdge-MCU-C51.uvproj (Keil C51 project)
│   ├── Core/                   # fe.h / fe.c / fe_hmac_sha256.c (pure C)
│   ├── Inc/                    # fe_ability.h / fe_data.h / fe_port.h
│   ├── Ability/                # ability_*.c (6)
│   ├── Data/                   # data_*.c (2)
│   └── User/                   # main.c / register.c / fe_port.c (port layer)
└── platformio_ide/             # VS Code + PlatformIO plugin project (STC platform + SDCC)
    ├── platformio.ini          # stc / stc89c52rc (or stc15f2k60s2)
    ├── .vscode/extensions.json # recommends PlatformIO IDE
    ├── include/                # fe.h / fe_ability.h / fe_data.h / fe_port.h / fe_hmac_sha256.h
    └── src/                    # reuses keil bare-metal C + SDCC fe_port (8051 register-level)
```

> C51 has no Arduino version; two bare-metal C toolchains: `keil/` (Keil C51) and `platformio_ide/` (SDCC, VS Code plugin); same capabilities and commands.

### 5. Usage

1. Open `keil/MDK-ARM/FasterEdge-MCU-C51.uvproj` with Keil C51 (uVision)
2. Implement the platform port in `User/fe_port.c`:
   - **UART**: timer-1 baud rate + serial interrupt (code skeleton in TODO comments)
   - **EEPROM**: reference implementations at the end of the file for **STC89/STC15 built-in IAP** and **AT89S52 + 24C02 I2C**
   - **Time**: timer-0 one-second interrupt counter
3. Build to HEX and flash the 8051
4. Use serial commands (default 115200)

**Serial command examples:**

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

### 6. C51 Adaptation Notes

| Difference | ESP32/ESP8266 | C51 (8051) |
|------------|---------------|-----------|
| Toolchain | PlatformIO (GCC) / Keil MDK | **Keil C51** |
| Types | `uint32_t` / `size_t` | `u8` / `u16` / `u32` / `u64` (`long long`) |
| int width | 32-bit | **16-bit** (code avoids relying on int width) |
| Storage | NVS / Flash | EEPROM (STC IAP or 24C02) |
| Network | yes | **no** (network capabilities removed) |
| RAM | KB~MB | **128-256B** (buffers shrunk to 96B, big tables in xdata) |
| C99 features | allowed | **C89-compatible** (no compound literals; static module tables) |

### 6-b. PlatformIO IDE Version (VS Code plugin)

`platformio_ide/` is the **SDCC compiler** version (PlatformIO STC platform) that reuses the keil C code with an SDCC-compatible `fe_port.c` (8051 register-level UART polling / baud timer; EEPROM and time remain TODO). No Keil needed — build and flash right from VS Code.

1. Install the **PlatformIO IDE** extension in VS Code (prompted when opening `platformio_ide/`)
2. Open the `platformio_ide/` directory
3. Click **Build** / **Upload** / **Serial Monitor** (115200) in the status bar

```bash
cd platformio_ide
pio run            # build (generates HEX)
pio run -t upload  # flash
pio device monitor # serial monitor
```

> To change chips, edit `board` in `platformio.ini` (e.g. `stc15f2k60s2`); enable `-Dprintf=printf_large` for `%lu` formatting. Serial commands are identical to the keil version.

### 7. Correspondence with the Main Repo

- Command names are **identical** to the main repo, structurally identical to MCU-ESP32/ESP8266
- `Atom` model: singleton global Atom with `data_` / `ability_` prefix routing
- Tokens use HMAC-SHA256 (pure C, no mbedTLS); secrets persisted in EEPROM
- Modbus register tables in RAM; RTU frame service entry `modbus_slave_service()` reserved

### 8. Sister Projects

- **[FasterEdge MCU - ESP32](https://github.com/FasterEdge/MCU-ESP32)**: 9 Abilities + 4 Data
- **[FasterEdge MCU - ESP8266](https://github.com/FasterEdge/MCU-ESP8266)**: 9 Abilities + 4 Data
- **[FasterEdge](https://github.com/FasterEdge/FasterEdge)**: main framework repo