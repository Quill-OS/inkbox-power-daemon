#include "cinematicBrightness.h"
#include "functions.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <cstdlib>
#include <cmath>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

const std::string emitter = "cinematicBrightness";
extern string model;

extern int cinematicBrightnessDelayMs;

void setBrightnessCin(int levelToSetCold, int levelToSetWarm, int currentLevelCold, int currentLevelWarm) {
  string cbPath = "/opt/bin/cinematic_brightness";
  const char * cbArgs[] = { cbPath.c_str(), std::to_string(levelToSetCold).c_str(), std::to_string(levelToSetWarm).c_str(), std::to_string(currentLevelCold).c_str(), std::to_string(currentLevelWarm).c_str(), std::to_string(cinematicBrightnessDelayMs * 100).c_str(), "0", nullptr };
  int fakePid = 0;
  posixSpawnWrapper(cbPath.c_str(), cbArgs, true, &fakePid);
}

void saveBrightness(int level, int type) {
  if(type == 0) {
    writeFileString("/tmp/savedBrightness", to_string(level));
  }
  else {
    writeFileString("/tmp/savedWarmth", to_string(level));
  }
}

int restoreBrightness(int type) {
  if(type == 0) {
    if (fileExists("/tmp/savedBrightness") == true) {
      return stoi(readConfigString("/tmp/savedBrightness"));
    } else {
      log("'/tmp/savedBrightness' doesn't exist, probably because of massive power button spamming. Returning current brightness", emitter);
      return getBrightness(0);
    }
  }
  else {
    if (fileExists("/tmp/savedWarmth") == true) {
      return stoi(readConfigString("/tmp/savedWarmth"));
    } else {
      log("'/tmp/savedWarmth' doesn't exist, probably because of massive power button spamming. Returning current brightness", emitter);
      return getBrightness(1);
    }
  }
}

void setBrightness(int device, int level, int type) {
  if(type != 0) {
    if(model == "n873" || model == "n418") {
      level = 10 - 0.1 * level;
    }
  }
  if (model == "n249") {
    if(type == 0) {
      writeFileString("/sys/class/backlight/backlight_cold/brightness", std::to_string(level));
    }
    else {
      writeFileString("/sys/class/backlight/backlight_warm/brightness", std::to_string(level));
    }
  } else if (model == "n418") {
    if(type == 0) {
      writeFileString("/sys/class/backlight/mxc_msp430.0/brightness", std::to_string(level));
    }
    else {
      writeFileString("/sys/class/leds/aw99703-bl_FL1/brightness", std::to_string(level));
    }
  } else if (model == "n873") {
    if(type == 0) {
      writeFileString("/sys/class/backlight/mxc_msp430.0/brightness", std::to_string(level));
    }
    else {
      writeFileString("/sys/class/backlight/lm3630a_led/color", std::to_string(level));
    }
  }
  else {
    if(type == 0) {
      ioctl(device, 241, level);
    }
  }
}

// Bugs?
int getBrightness(int type) {
  if (model == "n613") {
    return stoi(readConfigString("/opt/config/03-brightness/config"));
  } else if (model == "n236" or model == "n437") {
    return stoi(readConfigString("/sys/class/backlight/mxc_msp430_fl.0/brightness"));
  } else if (model == "n249") {
    if(type == 0) {
      return stoi(readConfigString("/sys/class/backlight/backlight_cold/actual_brightness"));
    }
    else {
      return stoi(readConfigString("/sys/class/backlight/backlight_warm/actual_brightness"));
    }
  } else if (model == "n418") {
    if(type == 0) {
      return stoi(readConfigString("/sys/class/backlight/mxc_msp430.0/actual_brightness"));
    }
    else {
      return 10 * (10 - stoi(readConfigString("/sys/class/leds/aw99703-bl_FL1/color")));
    }
  } else if (model == "n873") {
    if(type == 0) {
      return stoi(readConfigString("/sys/class/backlight/mxc_msp430.0/brightness"));
    }
    else {
      return 10 * (10 - stoi(readConfigString("/sys/class/backlight/lm3630a_led/color")));
    }
  } else {
    if(model != "n705" && model != "n905b" && model != "n905c" && model != "kt") {
      return stoi(readConfigString("/sys/class/backlight/mxc_msp430.0/actual_brightness"));
    }
    else {
      return 0;
    }
  }
}
