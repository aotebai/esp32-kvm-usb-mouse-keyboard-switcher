# ESP32-S3 KVM 切换器（双设备键鼠共享）

## 🌐 语言切换 / Language Switch
- [📝 中文版本](#中文版本)
- [🔤 English Version](#english-version)

---

## 中文版本

### 项目介绍
一款基于 ESP32-S3 和 CH9350 模块的双设备 KVM 切换器，可实现一套键鼠无缝共享给两台电脑，搭配ESP32S3板载RGB-LED 灯光特效指示工作状态，无需安装驱动，无开发经验用户也可通过预编译固件一键烧录使用。支持双设备供电冗余设计，确保单设备开机时系统仍能稳定运行。

### ✨ 核心功能
- 双设备切换：支持 2 台电脑之间的键鼠快速切换（共享一套键盘和鼠标）
- 两种切换方式：鼠标中键（可通过三位微动开关的K2键禁用）或三位微动开关 K1 键
- 灯光特效指示：切换时 LED 三种不重复的颜色顺序爆闪、 蓝色呼吸灯特效指示上位机 A，红色呼吸灯特效指示上位机 B，无频闪、无熄灭异常
- 核心功能完整：键鼠 DMA 透传，鼠标中键切换，三位微动开关 K1 键切换，K2键（开/关）鼠标中间键功能，K3键短按控制 LED、长按复位，静置后无功能衰减
- 供电冗余设计：通过肖特基二极管实现双设备供电互备，单设备开机即可保障系统正常运行
- 开发环境兼容：兼容 ESP-IDF v5.5.1，无第三方依赖，仅使用原生兼容 API，运行稳定
- 免驱兼容：支持所有支持 USB HID 协议的操作系统（Windows/Mac/Linux）
- 一键烧录：提供预编译固件，无需搭建 ESP-IDF 开发环境

### 🛠️ 硬件清单
| 硬件名称 | 购买链接 | 数量 | 单价(元) | 金额(元) | 快递费(元) | 合计(元) |
|----------|----------|------|----------|----------|------------|----------|
| ESP32 S3 核心板（1-N16R8） | [ESP32 S3 核心板链接](https://item.taobao.com/item.htm?id=715306783664) | 1 | 24.1 | 24.1 | 0 | 24.1 |
| CH9350 模块 | [CH9350 模块链接](https://item.taobao.com/item.htm?id=695173316772) | 3 | 18.5 | 55.5 | 3 | 58.5 |
| 三位微动开关（含 K1/K2/K3 按键） | [三位微动开关链接](https://item.taobao.com/item.htm?id=550176517768) | 1 | 3.88 | 3.88 | 0 | 3.88 |
| USB OTG 线 (公对公) | [USB OTG 线链接](https://item.taobao.com/item.htm?id=550176517768) | 6 | 0.85 | 5.1 | 2 | 7.1 |
| 直插肖特基二极管DO-41(1N5819 1A 40V) | [直插肖特基二极管链接](https://detail.tmall.com/item.htm?id=781535844592) | 20(使用2个) | 1.7 | 1.7 | 0 | 1.7 |
| 304不锈钢十圆头螺丝螺母套装 | [圆头螺丝螺母套装链接](https://detail.tmall.com/item.htm?id=637072501037) | 50(使用14个) | 4.7 | 4.7 | 0 | 4.7 |
| 3D打印外壳 | [3D打印链接](https://www.jlc-3dp.cn/fp/Amntaau/1) | 1 | 24.78 | 24.78 | 3.2 | 27.98 |

### 🔌 接线与组装
1. 将键盘、鼠标分别连接至 CH9350 模块的对应 USB 接口
2. CH9350模块 UART口(上位机A) 与 ESP32-S3 连接：TXD→U0RX (GPIO10)、RXD→U0TX (GPIO11) 、GND→GND、5V→1N5819→5V（确保供电稳定）
3. CH9350模块 UART口(上位机B) 与 ESP32-S3 连接：TXD→U2RX (GPIO44)、RXD→U2TX (GPIO43) 、GND→GND、5V→1N5819→5V（确保供电稳定）
4. 三位微动开关接线：K1/K2/K3 按键分别连接至 ESP32-S3 对应的 GPIO 引脚
5. 肖特基二极管:两个二极管正极分别接上位机A、上位机B的5V引脚，负极接ESP32S3的5Vin和下位机C的5V引脚(确保在一台计算机开机的情况下有效供电，整个系统能正常运行)
6. 将所有部件整理后，组装至 3D 打印外壳中，固定牢固避免接触不良

#### 接线示意图
##### 1. 电源部分接线



       [上位机 A]                       [上位机 B]
          |                               |
        5V_OUT                           5V_OUT
          |                               |
          +-----> [D1] +-----+            +-----> [D2] +-----+
          |       (1N5819)   |            |       (1N5819)   |
          |       DO-41      |            |       DO-41      |
          |                  |            |                  |
          +------------------+------------+------------------+
                             |
                        [系统 5V 总线]
                             |
                  +----------+-----------+
                  |                      |
            [ESP32S3]                [下位机 C]
            5V (Vin)                 5V_IN
            GND -------------------- GND

    两个二极管正极分别接上位机 A、上位机 B 的 5V 引脚，负极汇接到系统 5V 总线
    系统 5V 总线为 ESP32S3 和下位机 C 供电，确保单台计算机开机时系统仍能正常运行

##### 2. 串口与控制部分接线


       [ESP32S3]                  [上位机 A]
       U0TX (GPIO11) ------------> RXD
       U0RX (GPIO10) <------------ TXD
       
       U2TX (GPIO43) ------------> RXD  [上位机 B]
       U2RX (GPIO44) <------------ TXD
       
       U1TX (GPIO17) ------------> TXD  [下位机 C]
       U1RX (GPIO18) <------------ RXD
       
       GPIO12 (K1) <-------------- 微动开关 (GND)
       GPIO13 (K2) <-------------- 微动开关 (GND)
       GPIO14 (K3) <-------------- 微动开关 (GND)




### 🚀 一键烧录（无需 ESP-IDF）
#### 适用于 Windows 用户
1. 下载 [乐鑫 ESP Flash Download Tool](https://www.espressif.com/en/support/download/tools)（选择对应系统版本，建议下载最新版）
2. 将 ESP32-S3 开发板通过 USB 线连接电脑，选择对应的 COM 口（未识别请安装 ESP32-S3 专用 USB 驱动）
3. 设置烧录参数：`80MHz` / `DIO` / `2MB`（严格对应固件配置，避免烧录失败）
4. 添加固件文件并对应烧录地址：
    - `bootloader.bin` @ `0x0`
    - `partition-table.bin` @ `0x8000`
    - `ch9350_led_switch.bin` @ `0x10000`
5. 点击工具界面「START」按钮开始烧录，等待进度条完成（提示“FINISH”即为成功）

详细地址说明及参数核对：`firmware/precompiled/flash_addresses.md`

### 🎮 使用方法
1. 通过 USB 线将 ESP32-S3 分别连接至两台目标电脑，确保电脑正常识别设备（无驱动提示即可）
2. 设备切换：按下「鼠标中键」或「三位微动开关 K1 键」，LED 三种颜色顺序爆闪，完成后对应上位机呼吸灯亮起（蓝色为上位机 A，红色为上位机 B）
3. 鼠标中键功能控制：按下「K2 键」可切换鼠标中键切换功能的开启/关闭（建议搭配 LED 指示灯区分开关状态）
4. LED 控制与复位：短按「K3 键」可手动控制 LED 灯光开关；长按「K3 键」3 秒以上，设备复位至初始状态

### 🎨 3D 外壳与 CAD 图纸
- 3D 模型：`3d_models/`（含 FreeCAD 源文件及 STL 打印文件，可直接用于 3D 打印）
- 2D 平面图：`cad_drawing/`（DWG + DXF 格式，可用于加工或修改尺寸）

3D 外壳渲染图：
![3D 外壳渲染图](docs/screenshots/case_render.jpg)

### 🔨 从源码编译（开发者专用）
1. 安装 ESP-IDF v5.5.1 版本（严格对应版本，避免兼容性问题）
2. 克隆本仓库至本地：`git clone https://github.com/aotebai/esp32-kvm-usb-mouse-keyboard-switcher.git`
3. 进入项目目录，执行以下命令编译、烧录：
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### ❓ 常见问题（FAQ）
Q1：烧录失败 / 无法识别 COM 口
A：安装 ESP32-S3 对应 USB 驱动；更换优质 USB 线（避免数据线仅充电不传输数据）；更换电脑 USB 端口；确保开发板进入下载模式（按对应按键组合操作）。

Q2：组装后设备无响应、LED 不亮
A：检查接线（尤其是供电线路和 UART 串口连接，避免接反引脚）；验证固件烧录地址是否正确；确保 CH9350 模块获得稳定 5V 供电，ESP32-S3 供电电压正常；检查二极管方向是否接反。

Q3：切换功能正常，但键鼠无法被电脑识别
A：确认目标电脑将 ESP32-S3 识别为 USB HID 设备（设备管理器中可查看）；更换电脑 USB 端口或 USB 线；更新至项目最新版本固件，检查 CH9350 模块与 ESP32-S3 通信是否正常。

Q4：LED 灯光特效异常（频闪、颜色错误）
A：检查 LED 灯珠接线是否牢固、引脚对应正确；确认固件中 LED 控制逻辑与硬件接线一致；更换故障 LED 灯珠尝试。

Q5：单台电脑开机时系统无法工作
A：检查二极管接线方向是否正确（正极接上位机 5V，负极接系统总线）；确认二极管型号为 1N5819（确保正向导通压降低）；检查供电线路是否存在接触不良。

### 📺 演示视频
B 站演示链接（替换为你的实际视频链接）
### 📄 开源协议
Apache License 2.0（详见根目录 LICENSE 文件）

---
## english-version
### Project Introduction
A dual-device KVM switch based on ESP32-S3 and CH9350 modules, which enables seamless sharing of a single set of keyboard and mouse between two computers. Equipped with ESP32S3 on-board RGB-LED light effects to indicate the working status, no driver installation is required, and users without development experience can flash the precompiled firmware with one click. It supports dual-device power supply redundancy design to ensure stable system operation when a single device is powered on.
### ✨Key Features

- Dual-device switching: Supports fast switching of keyboard and mouse between 2 computers (shares one set of keyboard and mouse)
- Two switching methods: Mouse middle button (can be disabled via the K2 button of the 3-position microswitch) or K1 button of the 3-position microswitch
- LED effect indication: Three non-repeating colors of the LED flash in sequence during switching; a blue breathing light indicates Host A, and a red breathing light indicates Host B, with no flicker or abnormal extinction
- Complete core functions: Keyboard and mouse DMA transparent transmission, mouse middle button switching, K1 button switching of the 3-position microswitch, K2 button (on/off) for the mouse middle button function, K3 button short press to control the LED, long press to reset, and no functional degradation after standing idle
- Redundant power supply design: Achieves dual-device power supply mutual backup through Schottky diodes, ensuring stable system operation when a single device is powered on
- ESP-IDF compatibility: Compatible with ESP-IDF v5.5.1, no third-party dependencies, and only uses natively compatible APIs for stable operation
- Driver-free compatibility: Supports all operating systems (Windows/Mac/Linux) that support the USB HID protocol
- One-click flashing: Provides precompiled firmware, no need to build an ESP-IDF development environment

### 🛠️ Hardware List
| Hardware Name | Taobao Link | Quantity | Unit Price (CNY) | Total Amount (CNY) | Shipping Fee (CNY) | Grand Total (CNY) |
|:-------------:|:-----------:|:--------:|:----------------:|:------------------:|:------------------:|:-----------------:|
| ESP32 S3 Core Board (1-N16R8) | [ESP32 S3 Core Board Link](https://item.taobao.com/item.htm?id=715306783664) | 1 | 24.1 | 24.1 | 0 | 24.1 |
| CH9350 Module | [CH9350 Module Link](https://item.taobao.com/item.htm?id=695173316772) | 3 | 18.5 | 55.5 | 3 | 58.5 |
| 3-position Microswitch (with K1/K2/K3 Buttons) | [3-position Microswitch Link](https://item.taobao.com/item.htm?id=550176517768) | 1 | 3.88 | 3.88 | 0 | 3.88 |
| USB OTG Cable (Male to Male) | [USB OTG Cable Link](https://item.taobao.com/item.htm?id=550176517768) | 6 | 0.85 | 5.1 | 2 | 7.1 |
| Through-hole Schottky Diode DO-41 (1N5819 1A 40V) | [Through-hole Schottky Link](https://detail.tmall.com/item.htm?id=781535844592) | 20(use2) | 1.7 | 1.7 | 0 | 1.7 |
| 304 Stainless Steel Pan Head Screw and Nut Set | [Head Screw and Nut Set Link](https://detail.tmall.com/item.htm?id=637072501037) | 50(use14) | 4.7 | 4.7 | 0 | 4.7 |
| 3D Printed Case | [3D Printed Case Link](https://www.jlc-3dp.cn/fp/Amntaau/1) | 1 | 24.78 | 24.78 | 3.2 | 27.98 |

### 🔌 Wiring & Assembly

1.Connect the keyboard and mouse to the corresponding USB ports of the CH9350 module respectively
2.Connect the CH9350 module UART port (Host A) to ESP32-S3: TXD→U0RX (GPIO10), RXD→U0TX (GPIO11), GND→GND, 5V→1N5819→5V (ensure stable power supply)
3.Connect the CH9350 module UART port (Host B) to ESP32-S3: TXD→U2RX (GPIO44), RXD→U2TX (GPIO43), GND→GND, 5V→1N5819→5V (ensure stable power supply)
4.Wire the 3-position microswitch: Connect the K1/K2/K3 buttons to the corresponding GPIO pins of the ESP32-S3 respectively
5.Schottky diodes: The anodes of the two diodes are connected to the 5V pins of Host A and Host B respectively, and the cathodes are connected to the 5Vin of ESP32S3 and the 5V pin of Slave C (to ensure effective power supply when one computer is powered on and the entire system can run normally)
6.Organize all components and assemble them into the 3D printed case, fix them firmly to avoid poor contact

#### Wiring Diagrams
##### 1. Power Supply Wiring
 [Host A]                       [Host B]
          |                               |
        5V_OUT                           5V_OUT
          |                               |
          +-----> [D1] +-----+            +-----> [D2] +-----+
          |       (1N5819)   |            |       (1N5819)   |
          |       DO-41      |            |       DO-41      |
          |                  |            |                  |
          +------------------+------------+------------------+
                             |
                        [System 5V Bus]
                             |
                  +----------+-----------+
                  |                      |
            [ESP32S3]                [Slave C]
            5V (Vin)                 5V_IN
            GND -------------------- GND

##### 2. UART & Control Wiring
       [ESP32S3]                  [Host A]
       U0TX (GPIO11) ------------> RXD
       U0RX (GPIO10) <------------ TXD
       
       U2TX (GPIO43) ------------> RXD  [Host B]
       U2RX (GPIO44) <------------ TXD
       
       U1TX (GPIO17) ------------> TXD  [Slave C]
       U1RX (GPIO18) <------------ RXD
       
       GPIO12 (K1) <-------------- Microswitch (GND)
       GPIO13 (K2) <-------------- Microswitch (GND)
       GPIO14 (K3) <-------------- Microswitch (GND)

### 🚀 One-Click Flashing (No ESP-IDF Required)
#### For Windows Users

1. Download  [ Espressif ESP Flash Download Tool ](https://www.espressif.com/en/support/download/tools) (select the corresponding system version, it is recommended to download the latest version)
2. Connect the ESP32-S3 development board to the computer via a USB cable, and select the corresponding COM port (install the dedicated ESP32-S3 USB driver if it is not recognized)
3. Set the flashing parameters: 80MHz / DIO / 2MB (strictly correspond to the firmware configuration to avoid flashing failure)
4. Add firmware files and corresponding flashing addresses:
        - bootloader.bin @ 0x0
        - partition-table.bin @ 0x8000
        - ch9350_led_switch.bin @ 0x10000
5. Click the "START" button on the tool interface to start flashing, and wait for the progress bar to complete (the prompt "FINISH" indicates success)

For detailed address description and parameter verification: firmware/precompiled/flash_addresses.md
### 🎮 How to Use

1. Connect the ESP32-S3 to two target computers via USB cables respectively, ensuring the computers recognize the device normally (no driver prompt is required)
2. Device switching: Press the "mouse middle button" or "K1 button of the 3-position microswitch", the LED will flash three colors in sequence, and the corresponding host breathing light will turn on after completion (blue for Host A, red for Host B)
3. Mouse middle button function control: Press the "K2 button" to switch the on/off status of the mouse middle button switching function (it is recommended to use an LED indicator to distinguish the on/off status)
4. LED control and reset: Short press the "K3 button" to manually control the LED on/off; long press the "K3 button" for more than 3 seconds to reset the device to the initial state

### 🎨 3D Case & CAD Files

 - 3D Models: 3d_models/ (including FreeCAD source files and STL printing files, which can be directly used for 3D printing)
 - 2D Drawings: cad_drawing/ (DWG + DXF formats, which can be used for processing or size modification)

3D Case Rendering:

### 🔨 Build from Source (For Developers)

1. Install ESP-IDF v5.5.1 (strictly correspond to the version to avoid compatibility issues)
2. clone repositoryto locate:    `git clone https://github.com/aotebai/esp32-kvm-usb-mouse-keyboard-switcher.git`
3. Enter the project directory and execute the following commands to compile and flash:
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```
### ❓ Frequently Asked Questions (FAQ)
Q1: Flashing failed / COM port not recognized

A: Install the corresponding USB driver for ESP32-S3; replace with a high-quality USB cable (avoid data cables that only charge without transmitting data); replace the USB port of the computer; ensure the development board enters download mode (operate according to the corresponding key combination).

Q2: No response after assembly, LED not on

A: Check the wiring (especially the power supply line and UART connection, avoid reversing pins); verify that the firmware flashing address is correct; ensure the CH9350 module receives stable 5V power supply and the ESP32-S3 power supply voltage is normal; check if the diode is reversed.

Q3: Switching function is normal, but the keyboard and mouse are not recognized by the computer

A: Confirm that the target computer recognizes the ESP32-S3 as a USB HID device (can be viewed in Device Manager); replace the USB port or USB cable of the computer; update to the latest version of the project firmware, and check whether the communication between the CH9350 module and ESP32-S3 is normal.

Q4: Abnormal LED light effects (flicker, wrong color)

A: Check whether the LED wiring is firm and the pins are corresponding correctly; confirm that the LED control logic in the firmware is consistent with the hardware wiring; try replacing the faulty LED.

Q5: The system cannot work when a single computer is powered on

A: Check if the diode wiring direction is correct (anode connected to host 5V, cathode connected to system bus); confirm that the diode model is 1N5819 (to ensure low forward conduction voltage); check if there is poor contact in the power supply line.

### 📺 Demo Video
Bilibili Demo Link (Replace with your actual video link)
### 📄 Open Source License
Apache License 2.0 (See the LICENSE file in the root directory for details)
