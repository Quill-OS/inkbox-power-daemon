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

const std::string emitter = "cinematicBrightness";
extern string model;

extern int cinematicBrightnessDelayMs;

void setBrightnessCin(int levelToSet, int currentLevel) {
  if(model != "n705" && model != "n905b" && model != "n905c" && model != "kt") {
    int device = -1;
    if(model != "n249") {
      if ((device = open("/dev/ntx_io", O_RDWR)) == -1) {
        log("Error on opening ntx device", emitter);
      }
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
}

void saveBrightness(int level) {
  writeFileString("/tmp/savedBrightness", to_string(level));
}

int restoreBrightness() {
  if (fileExists("/tmp/savedBrightness") == true) {
    return stoi(readConfigString("/tmp/savedBrightness"));
  } else {
    log("'/tmp/savedBrightness' doesn't exist, probably because of massive power button spamming. Returning current brightness", emitter);
    return getBrightness();
  }
}

void setBrightness(int device, int level) {
  // TODO: Make this smarter
  if(model == "n249") {
    writeFileString("/sys/class/backlight/backlight_cold/brightness", std::to_string(level));
    writeFileString("/sys/class/backlight/backlight_warm/brightness", std::to_string(level));
  }
  else {
    ioctl(device, 241, level);
  }
}

// Bugs?
int getBrightness() {
  if (model == "n613") {
    return stoi(readConfigString("/opt/config/03-brightness/config"));
  } else if (model == "n236" or model == "n437") {
    return stoi(readConfigString("/sys/class/backlight/mxc_msp430_fl.0/brightness"));
  } else if (model == "n249") {
    return stoi(readConfigString("/sys/class/backlight/backlight_cold/actual_brightness"));
  } else {
    if(model != "n705" && model != "n905b" && model != "n905c" && model != "kt") {
      return stoi(readConfigString("/sys/class/backlight/mxc_msp430.0/actual_brightness"));
    }
    else {
      return 0;
    }
  }
}
