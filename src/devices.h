#ifndef DEVICES_H
#define DEVICES_H

#include <string>
using namespace std;

void manageChangeLedState();
void changeLedState();
void setLedState(bool on);
void ledManager();
void ledManagerAccurate();
bool getChargerStatus();
bool isDeviceChargerBug();
bool getAccurateChargerStatus();
void setCpuGovernor(string cpuGovernor);
void getLedPath();

#endif
