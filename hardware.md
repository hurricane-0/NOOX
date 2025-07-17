使用模组 ESP32-S3-WROOM-1

•384 KB ROM
•512 KB SRAM
•16 KB RTC SRAM
 模组内置16 MB FLASH 与 8 MB PSRAM

**不可用的引脚**
- **Strapping Pins (启动配置):** GPIO0, GPIO3, GPIO45, GPIO46。为保证100%启动稳定，完全弃用。
- **Internal PSRAM Pins :** GPIO35, GPIO36, GPIO37

板载RGB-WS2812,USB-OTG,USB转串口

外围：一个I2C接口用于OLED屏幕驱动为SSD1315，一个I2C接口预留，一个SPI接口用于TF卡，三个按键输入（高电平触发），三个LED输出（高电平点亮），一个额外的UART接口，三个通用GPIO预留

| 功能 (Function)    | 接口/编号 (Interface/ID) | ESP32-S3 引脚 | 备注 (Notes)                         |
| :--------------- | :------------------- | :---------- | :--------------------------------- |
| **I2C (OLED屏幕)** | `I2C0`               |             | 外部上拉电阻 4.7kΩ                       |
|                  | SDA                  | `GPIO4`     |                                    |
|                  | SCL                  | `GPIO5`     |                                    |
| **I2C (预留)**     | `I2C1`               |             | 外部上拉电阻 4.7kΩ                       |
|                  | SDA                  | `GPIO1`     |                                    |
|                  | SCL                  | `GPIO2`     |                                    |
| **SPI (TF卡)**    | `SPI2`               |             |                                    |
|                  | CS (片选)              | `GPIO6`    |                                    |
|                  | SCK (时钟)             | `GPIO15`    |                                    |
|                  | MISO (主入从出)          | `GPIO16`     |                                    |
|                  | MOSI (主出从入)          | `GPIO7`     |                                    |
| **按键输入**         | `Button 1`           | `GPIO21`    | **高电平触发。需配置为内部下拉输入或外部接下拉电阻**       |
|                  | `Button 2`           | `GPIO47`    | 同上                                 |
|                  | `Button 3`           | `GPIO38`    | 同上                                 |
| **LED 输出**       | `LED 1`              | `GPIO41`    | **高电平点亮。需要串联一个限流电阻 (例如 220Ω-1kΩ)** |
|                  | `LED 2`              | `GPIO40`    | 同上                                 |
|                  | `LED 3`              | `GPIO39`    | 同上                                 |
| **额外 UART**      | `UART1`              |             | 可用于与其他模块或传感器通信                     |
|                  | TXD                  | `GPIO17`    |                                    |
|                  | RXD                  | `GPIO18`    |                                    |
| **通用 GPIO (预留)** | `Reserved 1`         | `GPIO8`     |                                    |
|                  | `Reserved 2`         | `GPIO9`     |                                    |
|                  | `Reserved 3`         | `GPIO10`    |                                    |
| **RGB**          | `data`               | `GPIO48`    |                                    |
| **USB-OTG**      | `USB_D-`             | `GPIO19`    |                                    |
|                  | `USB_D+`             | `GPIO20`    |                                    |



