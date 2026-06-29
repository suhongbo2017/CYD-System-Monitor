#include "config.h"
#include "credentials.h"
#include <Arduino.h>

// Screen resolution
const uint16_t screenWidth = 240;
const uint16_t screenHeight = 320;

// Glances settings
String glances_host;
uint16_t glances_port;

// === High-Contrast Geek Theme ===
// Pure terminal-inspired dark theme with neon accent colors
const ThemeColors light_theme = {
    .bg_color = lv_color_hex(0x0A0A0A),
    .card_bg_color = lv_color_hex(0x141414),
    .text_color = lv_color_hex(0xFFFFFF),
    .cpu_color = lv_color_hex(0x00FF88),     // Neon Green
    .ram_color = lv_color_hex(0xFF6600),      // Neon Orange
    .border_color = lv_color_hex(0x2A2A2A)
};

const ThemeColors dark_theme = {
    .bg_color = lv_color_hex(0x000000),       // Pure Black
    .card_bg_color = lv_color_hex(0x0D0D0D),  // Near Black
    .text_color = lv_color_hex(0xFFFFFF),      // White
    .cpu_color = lv_color_hex(0x00FF88),       // Neon Green
    .ram_color = lv_color_hex(0xFF6600),       // Neon Orange
    .border_color = lv_color_hex(0x222222)     // Subtle border
};