#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

class WiFiManager {
public:
    static void begin();
    static void loop();
    static bool isConnected();
    static String getSSID();
    static IPAddress getIP();

private:
    static Preferences preferences;
    static DNSServer dnsServer;
    static WebServer server;
    static bool apMode;
    static String savedSSID;
    static String savedPassword;
    static unsigned long apStartTime;
    static const unsigned long AP_TIMEOUT_MS = 300000; // 5 minutes

    static bool loadCredentials();
    static void saveCredentials(const String &ssid, const String &password);
    static bool connectToWiFi(int timeoutMs = 10000);
    static void startAPMode();
    static void handleRoot();
    static void handleSave();
    static void handleScan();
    static void handleNotFound();
    static void handleCaptivePortal();
    static bool isCaptivePortal(const String &host);
    static String buildHtml();
};

#endif