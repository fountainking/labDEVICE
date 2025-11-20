# ESP-IDF Migration Reference

⚠️ **NOT FOR IMMEDIATE USE** - Reference for future migration from Arduino framework to ESP-IDF

## Why This Document Exists

The current LaboratoryM5 project uses the Arduino framework via PlatformIO. At some point, we may need to migrate to native ESP-IDF for:
- Better NAT/routing performance
- Lower-level WiFi control
- Advanced FreeRTOS features
- Reduced memory overhead

## Key Differences - Arduino vs ESP-IDF

### 1. Project Structure

```
Arduino:                 ESP-IDF:
src/main.cpp        →    src/main.c
platformio.ini      →    platformio.ini + CMakeLists.txt + sdkconfig
Arduino libs        →    Components in components/ directory
```

### 2. Build System

- **Arduino**: PlatformIO handles everything automatically
- **ESP-IDF**: Uses CMake + custom component system
- Each component needs `CMakeLists.txt` and `include/` subdirectory

### 3. Critical Migration Steps

**platformio.ini changes:**

```ini
[env:m5stick-c-plus2-espidf]
platform = espressif32
board = m5stick-c-plus2
framework = espidf
platform_packages =
    framework-espidf @ ~3.50102.0  ; ESP-IDF v5.1.2

build_flags =
    -DBOARD_HAS_PSRAM
    -Wno-format          ; Suppress printf format warnings for ESP logging

lib_deps =
    ; Arduino libs won't work - use ESP-IDF components instead
```

### 4. Code Changes

| Arduino                | ESP-IDF                          |
|------------------------|----------------------------------|
| `#include <WiFi.h>`      | `#include "esp_wifi.h"`            |
| `#include <WebServer.h>` | `#include "esp_http_server.h"`     |
| `Serial.println()`       | `ESP_LOGI(TAG, "...")`             |
| `setup() / loop()`       | `app_main()` + FreeRTOS tasks      |
| `delay(ms)`              | `vTaskDelay(pdMS_TO_TICKS(ms))`    |
| `WiFi.begin()`           | `esp_wifi_init()` + config structs |

### 5. Component Creation

For each custom module, create:

```
components/my_module/
├── CMakeLists.txt           # Build config
├── include/
│   └── my_module.h          # Public headers
└── my_module.c              # Implementation
```

**CMakeLists.txt template:**

```cmake
idf_component_register(
    SRCS "my_module.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_wifi lwip
)
```

### 6. Display/SPI Drivers

- **Arduino**: Use M5StickCPlus library
- **ESP-IDF**: Manual SPI configuration with `driver/spi_master.h`
- **CRITICAL**: Non-IOMUX pins limited to 26MHz max SPI clock
- **M5StickC Plus2 pins**: DC=14, RST=12, MOSI=15, SCLK=13, CS=5, BL=27

### 7. WiFi/Network Stack

```c
// ESP-IDF WiFi initialization is more verbose
esp_netif_init();
esp_event_loop_create_default();
esp_netif_create_default_wifi_ap();
esp_netif_create_default_wifi_sta();

wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);
esp_wifi_set_mode(WIFI_MODE_APSTA);
```

### 8. NAT Configuration

```c
#include "lwip/lwip_napt.h"

// Enable NAT on IP acquisition
ip_napt_enable_no(1, 1);  // softAP interface
```

### 9. Debugging

- **Arduino**: `Serial.println()`
- **ESP-IDF**: `ESP_LOGI()`, `ESP_LOGE()`, `ESP_LOGW()`
- Set log levels in `sdkconfig` or `idf.py menuconfig`

### 10. Common Gotchas

- **GPIO 23**: Avoid - triggers watchdog on M5StickC Plus2
- **String handling**: Use `snprintf()` instead of Arduino `String` class
- **Memory**: Check `esp_get_free_heap_size()` frequently
- **Task priority**: Higher numbers = higher priority in FreeRTOS
- **Stack size**: Default 4096 may be too small - increase for complex tasks

### 11. Build Commands

```bash
# Arduino framework (current):
pio run -t upload

# ESP-IDF framework (future):
pio run -e m5stick-c-plus2-espidf -t upload
pio device monitor
```

### 12. What Works in ESP-IDF NAT Router

✓ APSTA mode WiFi (simultaneous AP + STA)
✓ lwip NAT forwarding
✓ ST7789 display at 26MHz SPI
✓ Captive portal with DNS server
✓ TCP debug server
✓ FreeRTOS multi-tasking
✓ I2C devices (IMU 0x68, Power 0x51)
✓ Buzzer on GPIO 2

### 13. Reference Implementation

**Clone or reference:**
https://github.com/fountainking/LaboratoryStickC-NAT.git

All working examples of ESP-IDF components, CMakeLists, display drivers, and WiFi NAT configuration are in this repo.

---

## Migration Checklist (When Ready)

- [ ] Back up current Arduino implementation
- [ ] Create new `espidf` environment in `platformio.ini`
- [ ] Convert `src/*.cpp` to `src/*.c`
- [ ] Replace Arduino libraries with ESP-IDF components
- [ ] Create component structure for custom modules
- [ ] Port display driver to SPI master
- [ ] Port WiFi captive portal to ESP-IDF HTTP server
- [ ] Test NAT functionality
- [ ] Verify all features work
- [ ] Performance benchmarking vs Arduino

---

**Last Updated**: 2025-11-16
**Status**: Reference only - NOT for immediate implementation
