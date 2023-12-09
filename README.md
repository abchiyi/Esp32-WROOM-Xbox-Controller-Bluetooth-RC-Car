# Esp32-XboxController-Bluetooth-RC-Car

使用 xboxone 手柄控制 esp32 模块驱动的蓝牙小车

## 引脚说明

Pin 12 前进控制， 输出 PWM 信号，连接至驱动电调信号脚。

Pin 27 倒车控制，输出高/低电平，连接至驱动电调 F/R 引脚。

Pin 26 转向控制， 输出 PWM 信号，连接至一个 180° 舵机（例如 SG90）。

Pin 26 状态指示灯， 连接至 LED 正极， 持续慢闪-扫描/连接中，短双闪-已连接到 xbox 控制器。

## 如何连接

首先确保你的手柄固件是最新的， Xbox Accessories 应用 (可以从 Microsoft Store 获取)更新你的手柄固件。

在 Android 手机上下载 `nRF Connect` App，启动手柄。在 App 中的 SCANNER 分页中找到名为 `Xbox Wireless Controller` 的设备，在设备名称下的就是设备的蓝牙地址。

将你的 xbox 手柄的蓝牙地址填入`main.cpp`的这里

    19: static NimBLEAddress targetDeviceAddress("98:7A:14:29:10:41"); // 手柄蓝牙地址

烧录固件后启动 ESP32，长按 xbox 手柄配对按钮进入配对模式。当 Pin 26 连接的状态指示灯进入快闪状态，即表示连接成功。
