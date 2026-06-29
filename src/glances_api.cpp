#include "glances_api.h"
#include "gui.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>

bool GlancesAPI::fetchData(const char *endpoint, StaticJsonDocument<4096> &doc, int port_override)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    HTTPClient http;
    int port = port_override > 0 ? port_override : glances_port;
    String url = "http://" + glances_host + ":" + String(port) + endpoint;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DeserializationError error = deserializeJson(doc, payload);
    return !error;
}

void GlancesAPI::updateCPUData(StaticJsonDocument<4096> &doc)
{
    if (!fetchData("/api/4/cpu", doc))
        return;

    float cpuPercent = doc["total"].as<float>();
    int cpuCount = doc["cpucore"].as<int>();

    if (cpu_arc_obj.arc && cpu_arc_obj.label)
    {
        lv_obj_t **labels = (lv_obj_t **)lv_obj_get_user_data(cpu_arc_obj.arc);
        if (labels)
        {
            char buf[32];
            lv_label_set_text(labels[0], "CPU");
            snprintf(buf, sizeof(buf), "%d cores", cpuCount);
            lv_label_set_text(labels[1], buf);
            snprintf(buf, sizeof(buf), "%d%%", (int)cpuPercent);
            lv_label_set_text(labels[2], buf);
            lv_obj_set_style_text_font(labels[1], &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_font(labels[2], &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(labels[1], lv_color_hex(0x808080), 0);
            lv_obj_set_style_text_color(labels[2], lv_color_white(), 0);
        }
        set_arc_value_animated(cpu_arc_obj.arc, cpuPercent);
    }
}

void GlancesAPI::updateMemoryData(StaticJsonDocument<4096> &doc)
{
    if (!fetchData("/api/4/mem", doc))
        return;

    float memPercent = doc["percent"].as<float>();
    float totalRam = doc["total"].as<float>() / (1024.0 * 1024.0 * 1024.0);

    if (ram_arc_obj.arc && ram_arc_obj.label)
    {
        lv_obj_t **labels = (lv_obj_t **)lv_obj_get_user_data(ram_arc_obj.arc);
        if (labels)
        {
            char buf[32];
            lv_label_set_text(labels[0], "RAM");
            snprintf(buf, sizeof(buf), "%d%%", (int)memPercent);
            lv_label_set_text(labels[1], buf);
            snprintf(buf, sizeof(buf), "/ %.1f GB", totalRam);
            lv_label_set_text(labels[2], buf);
        }
        set_arc_value_animated(ram_arc_obj.arc, memPercent);
    }
}

void updateGlancesData()
{
    static unsigned long lastGlancesUpdate = 0;
    if (millis() - lastGlancesUpdate < GLANCES_UPDATE_INTERVAL)
    {
        return;
    }
    static StaticJsonDocument<4096> doc;

    // ── CPU & RAM ──
    GlancesAPI::updateCPUData(doc);
    GlancesAPI::updateMemoryData(doc);

    // ── Temperature ──
    // Try Glances sensors first, fall back to WMI temp server
    bool tempFound = false;
    if (GlancesAPI::fetchData("/api/4/sensors", doc))
    {
        for (JsonVariant sensor : doc.as<JsonArray>())
        {
            const char *label = sensor["label"].as<const char *>();
            if (!label) continue;
            if (strcmp(label, "Package id 0") == 0 ||
                strcmp(label, "Tdie") == 0 ||
                strcmp(label, "Tctl") == 0 ||
                (strstr(label, "CPU") != NULL))
            {
                int temp = (int)sensor["value"].as<float>();
                if (temp > 0 && temp < 120) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %d\u00B0C", temp);
                    update_compact_label(temp_label, buf);
                    tempFound = true;
                }
                break;
            }
        }
    }
    // Fallback: WMI temperature server (Libre Hardware Monitor)
    if (!tempFound) {
        if (GlancesAPI::fetchData("/api/temp", doc, 61209))
        {
            for (JsonVariant sensor : doc.as<JsonArray>())
            {
                float val = sensor["value"].as<float>();
                if (val > 0 && val < 120) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %d\u00B0C", (int)val);
                    update_compact_label(temp_label, buf);
                    tempFound = true;
                }
                break;
            }
        }
    }
    if (!tempFound) {
        update_compact_label(temp_label, LV_SYMBOL_WARNING " --\u00B0C");
    }

    // ── Disk: pick the most used mount point (works on both Linux & Windows) ──
    if (GlancesAPI::fetchData("/api/4/fs", doc))
    {
        const char *best_mount = NULL;
        float best_percent = 0;
        for (JsonVariant fs : doc.as<JsonArray>())
        {
            float pct = fs["percent"].as<float>();
            const char *mnt = fs["mnt_point"].as<const char *>();
            if (mnt && pct > best_percent) {
                // Skip virtual/special mounts on Windows
                if (strstr(mnt, "/rootfs") || mnt[0] == '/' || (strlen(mnt) == 2 && mnt[1] == ':')) {
                    best_percent = pct;
                    best_mount = mnt;
                }
            }
        }
        if (best_mount) {
            char buf[32];
            // Show short mount name
            const char *display_name = best_mount;
            if (strlen(best_mount) == 2 && best_mount[1] == ':')
                display_name = best_mount; // e.g. "C:"
            else if (strrchr(best_mount, '/'))
                display_name = strrchr(best_mount, '/') + 1;
            snprintf(buf, sizeof(buf), LV_SYMBOL_DRIVE " %s %.0f%%", display_name, best_percent);
            update_compact_label(disk_label, buf);
        }
    }

    // ── Cache / second disk ──
    if (GlancesAPI::fetchData("/api/4/fs", doc))
    {
        // Find the second-most-used mount
        const char *best_mount = NULL;
        float best_percent = 0;
        const char *second_mount = NULL;
        float second_percent = 0;

        // First find the most used
        for (JsonVariant fs : doc.as<JsonArray>())
        {
            float pct = fs["percent"].as<float>();
            const char *mnt = fs["mnt_point"].as<const char *>();
            if (mnt && pct > best_percent) {
                second_percent = best_percent;
                second_mount = best_mount;
                best_percent = pct;
                best_mount = mnt;
            } else if (mnt && pct > second_percent) {
                second_percent = pct;
                second_mount = mnt;
            }
        }

        const char *cache_mnt = second_mount ? second_mount : best_mount;
        float cache_pct = second_mount ? second_percent : best_percent;

        if (cache_mnt) {
            char buf[32];
            const char *display_name = cache_mnt;
            if (strlen(cache_mnt) == 2 && cache_mnt[1] == ':')
                display_name = cache_mnt;
            else if (strrchr(cache_mnt, '/'))
                display_name = strrchr(cache_mnt, '/') + 1;
            snprintf(buf, sizeof(buf), LV_SYMBOL_SAVE " %s %.0f%%", display_name, cache_pct);
            update_compact_label(cache_label, buf);
        }
    }

    // ── Uptime ──
    if (GlancesAPI::fetchData("/api/4/uptime", doc))
    {
        String payload = doc.as<String>();
        payload.replace("\"", "");
        char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_POWER " %s", payload.c_str());
        update_compact_label(uptime_label, buf);
    }

    // ── Network: find the interface with the most traffic (works on any OS) ──
    if (GlancesAPI::fetchData("/api/4/network", doc))
    {
        float best_recv = -1;
        float best_sent = 0;

        for (JsonVariant interface : doc.as<JsonArray>())
        {
            const char *name = interface["interface_name"].as<const char *>();
            if (!name) continue;
            // Skip loopback
            if (strstr(name, "Loopback") || strstr(name, "lo")) continue;
            // Skip virtual adapters with no traffic
            float recv = interface["bytes_recv_rate_per_sec"].as<float>();
            float sent = interface["bytes_sent_rate_per_sec"].as<float>();
            if (recv + sent > best_recv) {
                best_recv = recv;
                best_sent = sent;
            }
        }

        if (best_recv >= 0) {
            char down_str[16], up_str[16];
            auto formatSpeed = [](float bytes_per_sec, char *buffer)
            {
                if (bytes_per_sec > 1024 * 1024)
                    sprintf(buffer, "%.1fM", bytes_per_sec / (1024.0 * 1024.0));
                else if (bytes_per_sec > 1024)
                    sprintf(buffer, "%.1fK", bytes_per_sec / 1024.0);
                else
                    sprintf(buffer, "%.0fB", bytes_per_sec);
            };

            formatSpeed(best_recv, down_str);
            formatSpeed(best_sent, up_str);

            char buf[64];
            snprintf(buf, sizeof(buf), LV_SYMBOL_DOWNLOAD " %s  " LV_SYMBOL_UPLOAD " %s", down_str, up_str);
            update_compact_label(network_label, buf);
        }
    }

    // ── Load ──
    if (GlancesAPI::fetchData("/api/4/load", doc))
    {
        float load1 = doc["min1"].as<float>();
        char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " %.1f", load1);
        update_compact_label(load_label, buf);
    }

    lastGlancesUpdate = millis();
}