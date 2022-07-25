#include "afterSleep.h"
#include "AppsFreeze.h"
#include "Wifi.h"
#include "cinematicBrightness.h"
#include "devices.h"
#include "fbinkFunctions.h"
#include "functions.h"
#include "pipeHandler.h"
#include "usbnet.h"

#include <chrono>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <mtd/mtd-user.h>
#include <mutex>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

// Variables
extern bool reconnectWifi;
extern bool deepSleep;
extern bool deepSleepPermission;
extern string cpuGovernorToSet;
// Idle count, to reset it
extern int countIdle;
extern sleepBool sleepJob;
extern mutex sleep_mtx;
// Screen dump
extern FBInkDump dump;
extern bool darkMode;
extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;
extern mutex occupyLed;

// There is no way to stop the thread, so use this bool
bool dieAfter;

// Explanation why this code looks garbage
// Threads in C++ can't be killed from the outside, so its needed to check every step
// for a variable change.
// "Use another library!" No. thats such a simple program
// that it doesn't need it + other libraries do the same, for example
// boost::thread does exactly what I have described above, just in the
// background
// ~Szybet

// void checkExitAfter()
void CEA() {
  if (dieAfter == false) {
    manageChangeLedState();
    waitMutex(&sleep_mtx);
    if (sleepJob != After) {
      sleep_mtx.unlock();
      log("log: Terminating afterSleep");
      dieAfter = true;
    }
    sleep_mtx.unlock();
  }
}

void afterSleep() {
  log("Launching afterSleep");
  dieAfter = false;
  waitMutex(&occupyLed);

  // Don't put CEA here to avoid locking deepSleepPermission forever
  returnDeepSleep();

  // Very important
  int fd = open("/sys/power/state-extended", O_RDWR);
  write(fd, "0", 1);
  close(fd);

  CEA();
  if (dieAfter == false) {
    writeFileString("/tmp/sleep_mode", "false");
    restoreFbDepth();
  }

  CEA();
  if (dieAfter == false) {
    system("/sbin/hwclock --hctosys -u");
    clearScreen(darkMode);
  }

  // TODO: Handle lockscreen

  CEA();
  if (dieAfter == false) {
    unfreezeApps();
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    restorePipeSend();
  }

  CEA();
  if (dieAfter == false) {
    restoreFbink(darkMode);
  }

  CEA();
  if (dieAfter == false) {
    setBrightnessCin(restoreBrightness(), 0);
    remove("/tmp/savedBrightness");
  }

  if (reconnectWifi == true) {
    log("Reconnecting to wifi because of option '5 - wifiReconnect'");
    CEA();
    if (dieAfter == false) {
      turnOnWifi();
    }
  } else {
    log("Not reconnecting to Wi-Fi because of option '5 - wifiReconnect'");
  }

  CEA();
  if (dieAfter == false) {
    startUsbNet();
  }

  CEA();
  if (dieAfter == false) {
    waitMutex(&sleep_mtx);
    sleepJob = Nothing;
    sleep_mtx.unlock();
  }

  occupyLed.unlock();
  waitMutex(&currentActiveThread_mtx);
  currentActiveThread = Nothing;
  currentActiveThread_mtx.unlock();
  countIdle = 0;
  log("Exiting afterSleep");
}

void returnDeepSleep() {
  log("Returning from deep sleep");
  if (deepSleep == true) {
    remove("/data/config/20-sleep_daemon/SleepCall");
    setCpuGovernor(cpuGovernorToSet);
  }
  deepSleep = false;
  deepSleepPermission = true;
}
