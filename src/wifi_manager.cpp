#include "wifi_manager.h"

Preferences WiFiManager::preferences;
DNSServer WiFiManager::dnsServer;
WebServer WiFiManager::server(80);
bool WiFiManager::apMode = false;
String WiFiManager::savedSSID;
String WiFiManager::savedPassword;
unsigned long WiFiManager::apStartTime = 0;

void WiFiManager::begin()
{
    Serial.println("\n--- WiFi Manager ---");

    preferences.begin("wifi", false);

    // Try with saved credentials first (from previous web configuration)
    if (loadCredentials()) {
        Serial.printf("Found saved WiFi: %s\n", savedSSID.c_str());
        if (connectToWiFi(15000)) {
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            apMode = false;
            return;
        }
    }

    // Fallback: try credentials.h if it has valid SSID
    #ifdef CREDENTIALS_H
    if (strlen(WIFI_SSID) > 0 && strlen(WIFI_PASSWORD) > 0) {
        Serial.printf("Trying credentials.h: %s\n", WIFI_SSID);
        savedSSID = WIFI_SSID;
        savedPassword = WIFI_PASSWORD;
        if (connectToWiFi(15000)) {
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            saveCredentials(savedSSID, savedPassword);
            apMode = false;
            return;
        }
    }
    #endif

    // Start AP mode for web-based WiFi configuration
    Serial.println("Starting AP mode for WiFi configuration...");
    startAPMode();
}

void WiFiManager::loop()
{
    if (apMode) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

bool WiFiManager::loadCredentials()
{
    savedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    return savedSSID.length() > 0;
}

void WiFiManager::saveCredentials(const String &ssid, const String &password)
{
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    savedSSID = ssid;
    savedPassword = password;
}

bool WiFiManager::connectToWiFi(int timeoutMs)
{
    Serial.printf("Connecting to WiFi: %s", savedSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.hostname("systemmonitor");
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts * 500 < timeoutMs) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startAPMode()
{
    apMode = true;
    apStartTime = millis();

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("CYD-Config");

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Connect to 'CYD-Config' and open http://192.168.4.1");

    // DNS server for captive portal detection
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Web server routes
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.on("/scan", handleScan);
    server.onNotFound(handleNotFound);

    server.begin();
}

bool WiFiManager::isConnected()
{
    return WiFi.status() == WL_CONNECTED && !apMode;
}

String WiFiManager::getSSID()
{
    return savedSSID;
}

IPAddress WiFiManager::getIP()
{
    return WiFi.localIP();
}

// ===================== Build HTML Page =====================
// Uses String concatenation to avoid C++ raw string literal issues

String WiFiManager::buildHtml()
{
    String h;
    h.reserve(2500);

    h += "<!DOCTYPE html><html><head>";
    h += "<meta charset='UTF-8'>";
    h += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    h += "<title>CYD WiFi Config</title><style>";
    h += "*{box-sizing:border-box;margin:0;padding:0}";
    h += "body{font-family:sans-serif;background:#1a1a2e;color:#fff;";
    h += "display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}";
    h += ".card{background:#16213e;border-radius:16px;padding:32px;width:100%;max-width:420px}";
    h += "h1{font-size:22px;margin-bottom:8px;color:#e94560}";
    h += "p{color:#8899aa;font-size:14px;margin-bottom:24px}";
    h += "label{display:block;font-size:13px;color:#aabbcc;margin-bottom:4px;margin-top:14px}";
    h += "input{width:100%;padding:12px;border:2px solid #2a2a4e;border-radius:10px;";
    h += "background:#0f3460;color:#fff;font-size:15px;outline:none}";
    h += "input:focus{border-color:#e94560}";
    h += "button{width:100%;padding:14px;margin-top:24px;background:#e94560;color:#fff;";
    h += "border:none;border-radius:10px;font-size:16px;font-weight:600;cursor:pointer}";
    h += "button:hover{background:#d63851}button:disabled{opacity:0.5}";
    h += ".btn-link{display:block;text-align:right;font-size:13px;color:#e94560;cursor:pointer;margin-top:4px}";
    h += ".loading{display:none;text-align:center;margin-top:16px;color:#8899aa}";
    h += ".status{margin-top:16px;padding:12px;border-radius:8px;font-size:13px;display:none}";
    h += ".ok{display:block;background:#1a4a2e;color:#4ade80}.err{display:block;background:#4a1a1e;color:#f87171}";
    h += ".spinner{display:inline-block;width:18px;height:18px;border:3px solid #2a2a4e;";
    h += "border-top-color:#e94560;border-radius:50%;animation:spin .8s linear infinite;";
    h += "vertical-align:middle;margin-right:6px}";
    h += "@keyframes spin{to{transform:rotate(360deg)}}";
    h += "</style></head><body><div class='card'>";
    h += "<h1>WiFi Configuration</h1>";
    h += "<p>Configure your WiFi to connect CYD System Monitor</p>";
    h += "<form id='f' onsubmit='return sw(event)'>";
    h += "<label>WiFi SSID</label>";
    h += "<input type='text' id='ssid' placeholder='Type SSID or scan...' required>";
    h += "<span class='btn-link' onclick='scan()'>Scan networks</span>";
    h += "<label>Password</label>";
    h += "<input type='password' id='pwd' placeholder='Enter WiFi password'>";
    h += "<button type='submit' id='btn'>Save and Connect</button>";
    h += "</form>";
    h += "<div class='loading' id='loading'><span class='spinner'></span>Connecting...</div>";
    h += "<div id='status'></div></div>";
    h += "<script>";
    h += "function scan(){";
    h += "var s=document.getElementById('status');s.innerHTML='Scanning...';";
    h += "fetch('/scan').then(function(r){return r.json()}).then(function(d){";
    h += "if(d.networks&&d.networks.length){";
    h += "var s2='<select style=\"width:100%;padding:10px;";
    h += "border:2px solid #2a2a4e;border-radius:10px;background:#0f3460;color:#fff;font-size:15px\"";
    h += " onchange=\"document.getElementById(\\'ssid\\').value=this.value\">';";
    h += "s2+='<option value=\"\">-- Select --</option>';";
    h += "d.networks.forEach(function(n){";
    h += "s2+='<option value=\"'+n.ssid+'\">'+n.ssid+' ('+n.rssi+'dBm)</option>';";
    h += "});";
    h += "s2+='</select>';s.innerHTML=s2;s.className='status ok';";
    h += "}else{s.innerHTML='No networks found';s.className='status err';}";
    h += "});}";
    h += "function sw(e){";
    h += "e.preventDefault();";
    h += "var s=document.getElementById('ssid').value.trim();";
    h += "if(!s){alert('Enter SSID');return;}";
    h += "document.getElementById('loading').style.display='block';";
    h += "document.getElementById('btn').disabled=true;";
    h += "fetch('/save?ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(document.getElementById('pwd').value));";
    h += "}";
    h += "</script></body></html>";

    return h;
}

// ===================== HTTP Handlers =====================

void WiFiManager::handleRoot()
{
    server.send(200, "text/html", buildHtml());
}

void WiFiManager::handleSave()
{
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid.length() == 0) {
        server.send(400, "text/plain", "SSID is required");
        return;
    }

    Serial.printf("Saving WiFi credentials: %s\n", ssid.c_str());
    saveCredentials(ssid, password);

    server.send(200, "text/plain", "OK");
    delay(100);
    server.close();

    // Try to connect with the new credentials
    Serial.println("Attempting to connect with new credentials...");
    apMode = false;
    dnsServer.stop();
    server.stop();

    if (connectToWiFi(20000)) {
        Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("Failed to connect. Restarting AP mode...");
        startAPMode();
    }
}

void WiFiManager::handleScan()
{
    Serial.println("Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\"", "\\\""); // Escape quotes
        json += "{\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encrypted\":" + String((WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false") + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
}

void WiFiManager::handleNotFound()
{
    String host = server.hostHeader();
    if (isCaptivePortal(host)) {
        handleCaptivePortal();
    } else {
        server.send(404, "text/plain", "Not Found");
    }
}

bool WiFiManager::isCaptivePortal(const String &host)
{
    // Don't redirect if accessing the AP IP directly or known hostnames
    if (host.indexOf("192.168") == 0 || host.indexOf("cyd") == 0) {
        return false;
    }
    return host.length() > 0;
}

void WiFiManager::handleCaptivePortal()
{
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}