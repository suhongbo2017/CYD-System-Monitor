#ifndef GLANCES_API_H
#define GLANCES_API_H

#include <ArduinoJson.h>

struct GlancesAPI
{
    static bool fetchData(const char *endpoint, StaticJsonDocument<4096> &doc, int port_override = 0);
    static void updateCPUData(StaticJsonDocument<4096> &doc);
    static void updateMemoryData(StaticJsonDocument<4096> &doc);
};

void updateGlancesData();

#endif