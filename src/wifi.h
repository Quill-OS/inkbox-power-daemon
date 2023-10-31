#ifndef WIFI_H
#define WIFI_H

#include <string>

using namespace std;

void turnOffWifi();
void turnOnWifi();
bool loadModule(string path);

#endif
