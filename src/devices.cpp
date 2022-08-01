#include "devices.h"
#include "functions.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CM_nLED 101
#define CM_USB_Plug_IN 108

const std::string emitter = "devices";
extern string model;
extern bool ledUsage;
extern mutex occupyLed;
extern bool ledState;
extern string ledPath;

void manageChangeLedState() {
  if (ledUsage == true) {
    changeLedState();
  }
}

void changeLedState() {
  if(ledPath != "none") {
    if(ledPath == "ntx") {
      int light;
      if((light = open("/dev/ntx_io", O_RDWR)) == -1) {
        fprintf(stderr, "Error opening ntx_io device\n");
      }
      else {
        if(ledState == 1) {
          ioctl(light, CM_nLED, 0);
          ledState = 0;
        }
        else {
          ioctl(light, CM_nLED, 1);
          ledState = 1;
        }
        close(light);
      }
    } 
    else {
      string state = readConfigStringNoLog(ledPath);
      int dev = open(ledPath.c_str(), O_RDWR);
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
}

void setLedState(bool on) {
  if(ledPath != "none") {
    if(ledPath == "ntx") {
      int light;
      if((light = open("/dev/ntx_io", O_RDWR)) == -1) {
        fprintf(stderr, "Error opening ntx_io device\n");
      }
      else {
        if(on == true) {
          ioctl(light, CM_nLED, 1);
          ledState = 1;
        }
        else {
          ioctl(light, CM_nLED, 0);
          ledState = 0;
        }
      }
      close(light);
    }
    else {
      int dev = open(ledPath.c_str(), O_RDWR);
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

// At boot, do not depend on ledState
void ledManagerAccurate() {
  if (ledUsage == true) {
    occupyLed.lock();
    // To be sure ledState is true
    setLedState(false);
    if (getAccurateChargerStatus() == true) {
      setLedState(true);
    }
    else {
      setLedState(false);
    }
    occupyLed.unlock();
  }
}

bool getChargerStatus() {
  string chargerStatus;
  if (model == "kt") {
    chargerStatus = readFile("/sys/devices/system/yoshi_battery/yoshi_battery0/battery_status");
    return stoi(chargerStatus);
  } else {
    // Thanks to https://github.com/koreader/KoboUSBMS/blob/2efdf9d920c68752b2933f21c664dc1afb28fc2e/usbms.c#L148-L158
    int ntxfd;
    if((ntxfd = open("/dev/ntx_io", O_RDWR)) == -1) {
      fprintf(stderr, "Error opening ntx_io device\n");
      return false;
    }
    else {
      unsigned long ptr = 0U;
      ioctl(ntxfd, CM_USB_Plug_IN, &ptr);
      close(ntxfd);
      return !!ptr;
    }
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
    return stoi(chargerStatus);
  } else {
    chargerStatus = readFile("/sys/devices/platform/pmic_battery.1/power_supply/mc13892_bat/status");
    chargerStatus = normalReplace(chargerStatus, "\n", "");
    if (chargerStatus == "Discharging" or chargerStatus == "Not charging") {
      return false;
    }
    else {
      return true;
    }
  }
}

void setCpuGovernor(string cpuGovernor) {
  log("Setting CPU frequency governor to " + cpuGovernor, emitter);
  int dev = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", O_RDWR);
  int writeStatus = write(dev, cpuGovernor.c_str(), cpuGovernor.length());
  close(dev);
  log("Write status writing to 'scaling_governor' is: " + std::to_string(writeStatus), emitter);
}

void getLedPath() {
  if(model == "n705" or model == "n905b" or model == "n905c" or model == "n613") {
    ledPath =  "/sys/class/leds/pmic_ledsb/brightness";
  }
  else if(model == "n306" or model == "n873") {
    ledPath = "/sys/class/leds/GLED/brightness";
  }
  else if(model == "bpi") {
    ledPath = "/sys/devices/platform/leds/leds/bpi:red:pwr/brightness";
  }
  else if(model == "kt") {
    ledPath = "/sys/class/leds/pmic_ledsb/brightness";
  }
  else if(model == "n236" or model == "n437") {
    ledPath = "ntx";
  } 
  else {
    ledPath = "/sys/class/leds/pmic_ledsb/brightness";
  }
}
