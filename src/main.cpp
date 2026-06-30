#include <Arduino.h>
#include <WiFi.h>
#include "lvgl.h"
#include "config.h"
#include "display.h"
#include "gui.h"
#include "glances_api.h"
#include "settings_manager.h"
#include "web_server.h"
#include "credentials.h"
#include "wifi_manager.h"
#include "SPIFFS.h"

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n======================");
    Serial.println("Starting System Monitor");
    Serial.println("======================");

    // Step 1: WiFi Manager handles connection
    // Tries saved Preferences first, then credentials.h as fallback,
    // then starts AP mode (SSID: "CYD-Config") for web-based configuration
    Serial.println("Step 1: WiFi Manager...");
    WiFiManager::begin();

    // Step 2: Initialize display (skip if in AP mode - no display needed yet)
    // Actually always init display - AP mode info can be shown
    Serial.println("Step 2: Initializing display");
    lv_init();
    init_display();

#if LV_USE_LOG != 0
    lv_log_register_print_cb([](const char *buf)
                             {
        Serial.printf(buf);
        Serial.flush(); });
#endif

    create_system_monitor_gui();
    SettingsManager::begin();
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Only start web server if connected to WiFi
    if (WiFiManager::isConnected())
    {
        setupWebServer();
    }

    Serial.println("======================");
    Serial.println("Setup complete!");
    Serial.println("======================");
}

void loop()
{
    // Process WiFi Manager (DNS + web server in AP mode)
    WiFiManager::loop();

    lv_timer_handler();
    updateGlancesData();

    // Only handle main web server if connected
    if (WiFiManager::isConnected())
    {
        handleWebServer();
    }

    delay(1);
}
