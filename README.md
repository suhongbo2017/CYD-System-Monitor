# CYD System Monitor (ESP32 "Cheap Yellow Display")

A sleek system monitoring display powered by ESP32 that shows real-time system metrics from a [Glances](https://nicolargo.github.io/glances/) server. Features a customizable UI with dark/light theme support using LVGL graphics library and power-saving display controls.

![Unraid](Images/device.jpeg)

> **⚠️ NOTE for CYD PLUS (ESP32-2432S028R PLUS):** This board has significant differences from the standard CYD. See the [CYD PLUS Pitfalls](#-cyd-plus-pitfalls) section below.

---

## 📋 Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Setup](#setup)
- [Hardware Variants](#hardware-variants)
- [🥇 CYD PLUS Pitfalls](#-cyd-plus-pitfalls)
- [Home Assistant Integration](#home-assistant-integration)
- [API Endpoints](#api-endpoints)
- [Troubleshooting](#troubleshooting)

## Features

- Real-time monitoring of:
  - CPU usage with core count and load average
  - RAM utilization with total capacity
  - Disk array usage percentage
  - Cache usage percentage
  - System temperature with color-coded warnings
  - Network traffic (upload/download) with auto-scaling units (B/KB/MB)
  - System load
  - Uptime

- Web interface for configuration:
  - Real-time theme customization
  - System statistics dashboard
  - Glances server configuration
  - Device control and monitoring
  - Display power management

![Web UI](Images/web.png)

- Home Assistant integration:
  - REST API endpoints
  - Device status monitoring & control
  - Remote theme control

## Requirements

- ESP32 development board
- TFT display compatible with TFT_eSPI library
  - Standard CYD (ESP32-2432S028R): ST7789V + VSPI
  - **CYD PLUS** (ESP32-2432S028R **PLUS**): **ST7789V + HSPI** ← see pitfalls below
- Glances server running on the target system

## Setup

1. Clone this repository
2. Open the project in PlatformIO

   - Rename the `platformio.example.ini` file to `platformio.ini`
   - Edit the `platformio.ini` file to set the correct path to your Arduino libraries

3. Configure your TFT display settings:
   - Modify TFT_eSPI settings according to your display's configuration
   - **For CYD PLUS**: see [CYD PLUS Pitfalls](#-cyd-plus-pitfalls) below
   - Adjust screen resolution in config.h if needed:

   ```cpp
   extern const uint16_t screenWidth = 240;
   extern const uint16_t screenHeight = 320;
   ```

4. Configure your network settings in credentials.example.h:

   - Rename the file to `credentials.h`
   - Edit the file to set your WiFi SSID and password

   ```cpp
   const char* const WIFI_SSID = "your_ssid_here";
   const char* const WIFI_PASSWORD = "your_password_here";
   ```

5. Build and upload the project using PlatformIO

6. Set up your Glances server configuration in the web interface:

   - Access the web interface at the device's IP address
   - Configure the Glances server IP address and port
   - Choose theme colors if you want to change them
   - Save the configuration

### Hardware Variants

| Feature | Standard CYD (ESP32-2432S028R) | CYD PLUS (ESP32-2432S028R **PLUS**) |
|---------|-------------------------------|-------------------------------------|
| Display Driver | ST7789V | ST7789V (or ILI9341-compatible) |
| SPI Port | **VSPI** (SPI3) | **HSPI** (SPI2) |
| MOSI | GPIO 23 | GPIO 13 |
| MISO | GPIO 19 | GPIO 12 |
| SCLK | GPIO 18 | GPIO 14 |
| TFT_CS | GPIO 15 | GPIO 15 |
| TFT_DC | GPIO 2 | GPIO 2 |
| TFT_RST | GPIO 4 | GPIO 4 |
| Touch CS | GPIO 21 | GPIO 21 |
| Backlight | GPIO 27 | GPIO 27 |

---

## 🥇 CYD PLUS Pitfalls

This section documents ALL the pitfalls encountered while getting the CYD PLUS (ESP32-2432S028R + TPM408-2.8 module) to work.

### Pitfall #1: Wrong SPI Port (VSPI → HSPI)

**The biggest trap.** Standard CYD uses **VSPI** (MOSI=23, MISO=19, SCLK=18), but **CYD PLUS** uses **HSPI** (MOSI=13, MISO=12, SCLK=14).

If you use VSPI pins on a CYD PLUS, **the display will not respond at all** — all register reads return `0xFF`.

**Fix:** Add these to `build_flags` in `platformio.ini`:
```ini
-D USE_HSPI_PORT
-D TFT_MISO=12
-D TFT_MOSI=13
-D TFT_SCLK=14
```

### Pitfall #2: Wrong Display Driver (ILI9341 → ST7789V)

Despite what the flex cable markings or online descriptions might suggest, **most CYD PLUS boards actually use ST7789V**, not ILI9341. Using `ILI9341_DRIVER` will produce a garbled (花屏) display.

The display module (TPM408-2.8) is labeled as ILI9341-compatible, but the actual driver IC on the flex cable may be ST7789V. Both drivers can co-exist on similar hardware.

**Fix:** Use ST7789V driver in `platformio.ini`:
```ini
-D ST7789_DRIVER
```

> ⚠️ **If ST7789V doesn't work**: Some batches may use actual ILI9341. Try `ILI9341_DRIVER` instead.

### Pitfall #3: Wrong Rotation for ILI9341

If your display DOES use ILI9341, the rotation setting is critical:
- `setRotation(3)` = landscape (320×240), USB on right
- `setRotation(1)` = landscape (320×240), USB on left
- `setRotation(0)` = portrait (240×320)

ILI9341 rotation 3 uses `MADCTL = MX | MY | MV | BGR`, while ST7789 rotation 3 uses `MADCTL = MV | MY | BGR`. The difference is **MX (Column Address Order)**.

### Pitfall #4: LVGL Byte Swap (`LV_COLOR_16_SWAP`)

**Critical for correct colors when using LVGL!**

LVGL stores pixels internally as BGR565 bitfield. When TFT_eSPI sends pixels over SPI, the byte order must match what the display expects. **If colors appear garbled (花屏) or wrong, check this setting:**

```c
// lv_conf.h
#define LV_COLOR_16_SWAP 0  // ← Must be 0 for TFT_eSPI

// display.cpp
tft.setSwapBytes(true);     // ← Must be true for LVGL compatibility
```

**Why:** With `LV_COLOR_16_SWAP=1`, LVGL swaps the bytes of each pixel before passing to the flush callback. But TFT_eSPI also handles byte ordering internally via `setSwapBytes()`. Having both enabled causes **double byte-swapping** — every pixel's high and low bytes get swapped twice, producing completely garbled colors.

The correct configuration is:
- `LV_COLOR_16_SWAP 0` in lv_conf.h (LVGL does NOT swap)
- `tft.setSwapBytes(true)` in display.cpp (TFT_eSPI swaps bytes correctly)

### Pitfall #5: DMA Conflicts with WiFi

TFT_eSPI's `pushImageDMA()` uses the same DMA engine that WiFi needs. When both are active, **display corruption can occur** (partial garbled output, tearing).

**Fix:** Use blocking `pushImage()` instead of DMA:
```cpp
// DON'T use:
tft.initDMA();
tft.pushImageDMA(x, y, w, h, data);

// DO use:
tft.pushImage(x, y, w, h, data);
```

### Pitfall #6: SPI Frequency Too High

The flex cable connection on CYD PLUS is sensitive to SPI speed. High frequencies (>40MHz) can cause signal integrity issues, especially with long FPC cables.

**Recommended settings:**
```ini
-D SPI_FREQUENCY=27000000      ; 27MHz write
-D SPI_READ_FREQUENCY=16000000  ; 16MHz read
```

If you still see random garbled pixels, try lowering to 20MHz/10MHz:
```ini
-D SPI_FREQUENCY=20000000
-D SPI_READ_FREQUENCY=10000000
```

### Pitfall #7: Backlight GPIO

The CYD PLUS backlight is controlled by **GPIO 27**. On some variants it's active HIGH, on others it might be active LOW.

```cpp
#define TFT_BL 27
#define TFT_BACKLIGHT_ON HIGH   // Try LOW if backlight doesn't turn on
pinMode(TFT_BL, OUTPUT);
digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
```

### Pitfall #8: LVGL Display Buffer Sizing

LVGL needs sufficient buffer space for smooth rendering. Too small results in tearing; too large wastes RAM.

**Recommended:**
```cpp
uint32_t buf_size = screenWidth * 20;  // 20 lines = 4800 pixels
```

Use `heap_caps_malloc` with `MALLOC_CAP_DMA` for proper alignment:
```cpp
buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
```

### Pitfall #9: TFT Reset Pin

Some CYD variants need a hardware reset via GPIO 4 before the display initializes. Ensure `TFT_RST=4` is defined:
```ini
-D TFT_RST=4
```

### Pitfall #10: Display Module Identification

The TPM408-2.8 module's flex cable has markings that can help identify the driver:

| Marking | Likely Driver |
|---------|--------------|
| "IL" on IC | ILI9341 (or ILItek compatible) |
| No "IL" marking | Could be ST7789V |

**If you're unsure which driver your display uses**, try this diagnostic test:
1. Build the simple TFT rotation test (no LVGL)
2. Test both `ILI9341_DRIVER` and `ST7789_DRIVER`
3. The correct driver will show clean quadrants (red/green/blue/yellow) without garbled output

### Pitfall #11: Screen Resolution in config.h

The original code defines:
```cpp
extern const uint16_t screenWidth = 240;
extern const uint16_t screenHeight = 320;
```

These are **portrait** dimensions (the panel is physically 240×320). After `setRotation(3)` (landscape), the active area is 320×240. LVGL must use `hor_res = screenHeight` and `ver_res = screenWidth`.

---

## Home Assistant Integration

The device exposes REST API endpoints for Home Assistant integration:

- GET `/api/status` - Device status and metrics
- POST `/api/command` - Control endpoints for theme switching and device restart

#### Easy Integration

A complete Home Assistant configuration example is provided in [homeassistant_example.yml](homeassistant_example.yml). This includes:

- System sensors (temperature, memory, WiFi signal, uptime)
- Binary sensors for dark mode and display state
- Switches for controlling dark mode and display power
- Commands for device restart and theme reset

Simply copy the configuration, replace `YOUR.DEVICE.IP.HERE` with your device's IP address, and add it to your Home Assistant configuration.
You should see the entities show up in home assistant after a restart.

## API Endpoints

### Web Interface Endpoints

- GET `/settings` - Returns:
  - Current device settings and theme colors
  - System metrics (CPU, memory, temperature)
  - Network information
  - Device information (chip model, SDK version, etc.)
  - Hardware statistics (heap, PSRAM, flash)

- POST `/settings` - Update device settings:
  - Theme colors
  - Dark/light mode
  - Glances server configuration

- POST `/restart` - Restart device
- POST `/resetTheme` - Reset theme to defaults
- POST `/displaySleep` - Control display power state:

  ```json
  {
    "sleep": true|false
  }
  ```

### Home Assistant Endpoints

- GET `/api/status` - Returns:
  - Temperature
  - Free heap memory
  - WiFi signal strength
  - Uptime
  - Dark mode state
  - Display state

- POST `/api/command` - Control endpoints for:
  - Theme switching (`dark_mode`: true|false)
  - Display power (`display`: true|false)
  - Device restart (`restart`: true)
  - Theme reset (`reset_theme`: true)

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| White screen (no image) | Wrong SPI port | Use `USE_HSPI_PORT` + correct pins for CYD PLUS |
| White screen (no image) | Wrong display driver | Try both `ILI9341_DRIVER` and `ST7789_DRIVER` |
| Garbled/colorful mess | Wrong display driver | Switch between ILI9341 and ST7789 |
| Garbled colors | LVGL byte swap mismatch | Set `LV_COLOR_16_SWAP 0` + `setSwapBytes(true)` |
| Random pixel corruption | DMA + WiFi conflict | Use `pushImage()` instead of `pushImageDMA()` |
| Random glitches | SPI frequency too high | Lower to 20MHz-27MHz |
| Partially garbled | Wrong rotation | Test all 4 rotations (0-3) |
| Black screen, no light | Backlight polarity wrong | Try `LOW` instead of `HIGH` |
| Display works but LVGL GUI is garbled | LV_COLOR_16_SWAP wrong | Set to 0, add setSwapBytes(true) |

## Contributing

Feel free to submit issues, fork the repository, and create pull requests for any improvements.