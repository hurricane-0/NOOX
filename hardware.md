使用模组 ESP32-S3-WROOM-1

•384 KB ROM
•512 KB SRAM
•16 KB RTC SRAM
 模组内置16 MB FLASH 与 8 MB PSRAM

内置PCB天线

以下是基于 ESP32-S3-WROOM-1 模组的初步外设引脚分配。请注意，这些是建议值，最终设计时可能需要根据实际 PCB 布线、模组引脚可用性以及避免与启动模式引脚冲突等因素进行调整。

**重要提示：**
*   **GPIO6-11** 通常连接到模组内部的 SPI Flash/PSRAM，不建议作为通用 GPIO 使用。
*   **GPIO19 和 GPIO20** 是 USB OTG 的专用引脚，请勿用于其他功能。
*   **高电平触发按键** 意味着按键按下时引脚为高电平，未按下时为低电平。软件中需要配置为下拉输入。

---

### **外设引脚分配**

1.  **屏幕：1.8寸 分辨率128x160 RGB_TFT (驱动 IC: ST7735s)**
    *   **接口：** 4线 SPI (假设为单向写入，无需 MISO)
    *   `SPI_CLK` (SCK): GPIO18
    *   `SPI_MOSI` (SDA/DIN): GPIO23
    *   `LCD_CS` (片选): GPIO5
    *   `LCD_DC` (数据/命令选择): GPIO2
    *   `LCD_RST` (复位): GPIO4
    *   `LCD_BL` (背光控制): GPIO45 (支持 PWM 调光)

2.  **按键：高电平触发 x 3**
    *   `KEY1`: GPIO47
    *   `KEY2`: GPIO41
    *   `KEY3`: GPIO33
    *   **配置：** 软件中配置为下拉输入。

3.  **LED：x 2**
    *   `LED1`: GPIO46
    *   `LED2`: GPIO42
    *   **配置：** 高电平点亮

4.  **RGB-WS2812：x 1**
    *   `WS2812_DATA`: GPIO48 (使用 RMT 外设驱动)

5.  **SD 卡槽：x 1**
    *   **接口：** SPI (使用 SPI2 或通用 GPIO 模拟 SPI)
    *   `SD_CLK`: GPIO36
    *   `SD_MOSI`: GPIO35
    *   `SD_MISO`: GPIO37
    *   `SD_CS`: GPIO34

6.  **I2C：x 1**
    *   `I2C_SDA`: GPIO7
    *   `I2C_SCL`: GPIO17

7.  **串口：x 2**
    *   **UART1:**
        *   `UART1_TX`: GPIO43
        *   `UART1_RX`: GPIO44
    *   **UART2:**
        *   `UART2_TX`: GPIO39
        *   `UART2_RX`: GPIO40

8.  **USB：**
    *   `USB_D-`: GPIO19 (专用)
    *   `USB_D+`: GPIO20 (专用)

---

**GPIO 若干：**
除了上述分配，ESP32-S3 还有其他可用的通用 GPIO 引脚，例如 GPIO0, GPIO1, GPIO3, GPIO8, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO21, GPIO22, GPIO24, GPIO25, GPIO26, GPIO27, GPIO28, GPIO29, GPIO30, GPIO31, GPIO32, GPIO41。
**请注意：**
*   GPIO0 是启动模式引脚，通常用于进入烧录模式，不建议作为普通功能引脚。
*   GPIO1 和 GPIO3 是默认的 UART0 TX/RX，如果 USB CDC 已经提供调试串口，这两个引脚可以作为通用 GPIO 使用，但需注意启动时的状态。
*   GPIO8, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16 属于内部 Flash/PSRAM 的 SPI 总线，对于 ESP32-S3-WROOM-1 模组，这些引脚通常不引出或不建议作为通用 GPIO 使用。