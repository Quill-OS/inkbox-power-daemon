#include "cinematicBrightness.h"
#include "functions.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <cstdlib>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern string model;

extern int cinematicBrightnessDelayMs;

void setBrightnessCin(int levelToSet, int currentLevel) {
  int device;
  if ((device = open("/dev/ntx_io", O_RDWR)) == -1) {
    log("Error on opening ntx device");
    exit(EXIT_FAILURE);
  }
  chrono::milliseconds timespan(cinematicBrightnessDelayMs);
  while (currentLevel != levelToSet) {
    if (currentLevel < levelToSet) {
      currentLevel = currentLevel + 1;
    } else {
      currentLevel = currentLevel - 1;
    }

    setBrightness(device, currentLevel);
    this_thread::sleep_for(timespan);
  }
  close(device);
}

void saveBrightness(int level) {
  writeFileString("/tmp/savedBrightness", to_string(level));
}

int restoreBrightness() {
  if (fileExists("/tmp/savedBrightness") == true) {
    return stoi(readConfigString("/tmp/savedBrightness"));
  } else {
    log("'/tmp/savedBrightness' doesn't exist, probably because of massive power button spamming. Returning current brightness");
    return getBrightness();
  }
}

void setBrightness(int device, int level) { ioctl(device, 241, level); }

// Bugs?
int getBrightness() {
  if (model == "n613") {
    return stoi(readConfigString("/opt/config/03-brightness/config"));
  } else if (model == "n236" or model == "n437") {
    return stoi(readConfigString("/sys/class/backlight/mxc_msp430_fl.0/brightness"));
  } else {
    return stoi(readConfigString("/sys/class/backlight/mxc_msp430.0/actual_brightness"));
  }
}
