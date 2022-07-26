#include "devices.h"
#include "functions.h"
#include <fcntl.h>
#include <unistd.h>

const std::string emitter = "devices";
extern string model;
extern bool ledUsage;
extern mutex occupyLed;
extern bool ledState;

void manageChangeLedState() {
  if (ledUsage == true) {
    changeLedState();
  }
}

void changeLedState() {
  if (model == "n306") {
    // Here, it's checking the real one; if some other app wants to control the LED, fine
    string path = "/sys/devices/platform/leds/leds/GLED/brightness";
    string state = readConfigStringNoLog(path);
    int dev = open(path.c_str(), O_RDWR);
    if (state == "1") {
      write(dev, "0", 1);
      ledState = 0;
    } else {
      write(dev, "1", 1);
      ledState = 1;
    }
    close(dev);
  }
}

void setLedState(bool on) {
  if (model == "n306") {
    string path = "/sys/devices/platform/leds/leds/GLED/brightness";
    int dev = open(path.c_str(), O_RDWR);
    if (on == true) {
      write(dev, "1", 1);
      ledState = 1;
    } else {
      write(dev, "0", 1);
      ledState = 0;
    }
    close(dev);
  }
}

void ledManager() {
  if (ledUsage == true) {
    if (occupyLed.try_lock() == true) {
      occupyLed.unlock();
      if (getAccurateChargerStatus() == true) {
        if (ledState == 0) {
          setLedState(true);
        }
      } else {
        if (ledState == 1) {
          setLedState(false);
        }
      }
    }
  }
}

bool getChargerStatus() {
  string chargerStatus;
  if (model == "kt") {
    chargerStatus = readFile("/sys/devices/system/yoshi_battery/yoshi_battery0/battery_status");
  } else {
    chargerStatus = readFile("/sys/devices/platform/pmic_battery.1/power_supply/mc13892_bat/status");
  }
  chargerStatus = normalReplace(chargerStatus, "\n", "");
  if (chargerStatus == "Discharging") {
    return false;
  } else {
    return true;
  }
}

bool isDeviceChargerBug() {
  if (model == "n905c" or model == "n905b" or model == "n705" or model == "n613" or model == "n236" or model == "kt") {
    return true;
  } else {
    return false;
  }
}

/*
  Let me explain this function.

  There are 3 states:
    1 Charging
    2 Discharging
    3 Not charging

  getChargerStatus() connects 2 and 3 in one, and is fine for some functions and
  applications when we need to know whether the charger is still connected or not.

  But now we need to know if the LED should be on or off, so here we are connecting 2 and 3.
*/
bool getAccurateChargerStatus() {
  string chargerStatus;
  if (model == "kt") {
    chargerStatus = readFile("/sys/devices/system/yoshi_battery/yoshi_battery0/battery_status");
  } else {
    chargerStatus = readFile("/sys/devices/platform/pmic_battery.1/power_supply/mc13892_bat/status");
  }
  chargerStatus = normalReplace(chargerStatus, "\n", "");
  if (chargerStatus == "Discharging" or chargerStatus == "Not charging") {
    return false;
  } else {
    return true;
  }
}

void setCpuGovernor(string cpuGovernor) {
  log("Setting CPU frequency governor to " + cpuGovernor, emitter);
  int dev = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", O_RDWR);
  int writeStatus = write(dev, cpuGovernor.c_str(), cpuGovernor.length());
  close(dev);
  log("Write status writing to 'scaling_governor' is: " + std::to_string(writeStatus), emitter);
}
