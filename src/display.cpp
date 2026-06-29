#include "display.h"
#include <Arduino.h>

#define TFT_BL 27
#define TFT_BACKLIGHT_ON HIGH

TFT_eSPI tft = TFT_eSPI();
lv_disp_draw_buf_t draw_buf;
lv_color_t *buf1;
lv_color_t *buf2;

void init_display()
{
    Serial.println("  - Turn on backlight...");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

    Serial.println("  - tft.begin()...");
    tft.begin();
    Serial.println("  - tft.begin() done");

    Serial.println("  - setRotation(3)...");
    tft.setRotation(3);

    // IMPORTANT: Enable byte swap for LVGL's pixel format compatibility
    // LVGL stores colors in BGR565 bitfield; TFT_eSPI sends RGB565 over SPI
    // Without this, colors appear garbled when using LVGL + TFT_eSPI together
    tft.setSwapBytes(true);

    // Use blocking pushImage instead of DMA to avoid memory corruption with WiFi
    // tft.initDMA();  // DMA disabled - can cause issues with concurrent WiFi operations

    tft.fillScreen(TFT_BLACK);

    // Allocate larger display buffers for LVGL (20 lines instead of 10)
    extern const uint16_t screenHeight;
    extern const uint16_t screenWidth;

    uint32_t buf_size = screenWidth * 20;  // 20 lines of pixels
    buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!buf1 || !buf2) {
        Serial.println("WARNING: Failed to allocate large display buffers, falling back to smaller ones");
        if (buf1) free(buf1);
        if (buf2) free(buf2);
        buf_size = screenWidth * 5;
        buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_size);
        buf2 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_size);
    }

    Serial.printf("  - Display buffers: %d pixels each\n", buf_size);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenHeight;  // 320 (landscape width)
    disp_drv.ver_res = screenWidth;   // 240 (landscape height)
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("  - init_display done");
}

void display_sleep(bool sleep)
{
    if (sleep)
    {
        digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
        tft.writecommand(0x28);  // DISPOFF
        delay(100);
        tft.writecommand(0x10);  // SLPIN
    }
    else
    {
        tft.writecommand(0x11);  // SLPOUT
        delay(100);
        tft.writecommand(0x29);  // DISPON
        delay(100);
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    }
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Use blocking pushImage instead of DMA to avoid corruption with WiFi active
    tft.startWrite();
    tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}