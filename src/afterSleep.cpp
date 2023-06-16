#include "afterSleep.h"
#include "appsFreeze.h"
#include "wifi.h"
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
const std::string emitter = "afterSleep";
extern bool reconnectWifi;
extern bool deepSleep;
extern bool deepSleepPermission;
extern string cpuGovernorToSet;
extern bool lockscreen;
extern string lockscreenBackgroundMode;
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
      log("Terminating afterSleep", emitter);
      dieAfter = true;
    }
    sleep_mtx.unlock();
  }
}

void afterSleep() {
  log("Launching afterSleep", emitter);
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
    string hwclockPath = "/sbin/hwclock";
    const char *args[] = {hwclockPath.c_str(), "--hctosys", "-u", nullptr};
    int fakePid = 0;
    posixSpawnWrapper(hwclockPath.c_str(), args, true, &fakePid);
    if(lockscreen == false and lockscreenBackgroundMode != "screensaver") {
      clearScreen(darkMode);
    }
  }

  // Moving Wi-Fi before apps because the GUI status icon would freak out
  // This will run in the background - even if lockscreen is launched
  if (reconnectWifi == true) {
    log("Reconnecting to Wi-Fi because of option '5 - wifiReconnect'", emitter);
    CEA();
    if (dieAfter == false) {
      turnOnWifi();
    }
  } else {
    log("Not reconnecting to Wi-Fi because of option '5 - wifiReconnect'", emitter);
  }

  CEA();
  if (dieAfter == false) {
    if(lockscreen == true) {
      // Overall lockscreen - this will catch those 2 scripts and one binary - they should be killed anyway
      if(getPidByName("lockscreen") == -1) {
        launchLockscreen();
      }
    }
  }

  CEA();
  if (dieAfter == false) {
    // Needed by lockscreen but takes time if lockscreen is off
    if(lockscreen == true) {
      setBrightnessCin(restoreBrightness(), 0);
      remove("/tmp/savedBrightness");
    }
  }

  // Here it waits for lockscreen to exit
  if(lockscreen == true) {
    while(dieAfter == false and getPidByName("lockscreen") != -1) {
      CEA();
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      log("Waiting for launch_lockscreen.sh to finish", emitter);
    }
    log("Exitin waiting for lockscreen", emitter);
  }

  CEA();
  if (dieAfter == false) {
    restoreFbink(darkMode);
    // It has a delay?
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
  }

  CEA();
  if (dieAfter == false) {
    unfreezeApps();
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    restorePipeSend();
  }

  CEA();
  if (dieAfter == false) {
    if(lockscreen == false) {
      setBrightnessCin(restoreBrightness(), 0);
      remove("/tmp/savedBrightness");
    }
  }

  // After lockscreen for a bit more security
  // And after everything else for speed
  CEA();
  if (dieAfter == false) {
    startUsbNet();
  }

  // Finish...
  CEA();
  if (dieAfter == false) {
    waitMutex(&sleep_mtx);
    sleepJob = Nothing;
    sleep_mtx.unlock();
  }
  // Everything after this will well, die

  occupyLed.unlock();
  waitMutex(&currentActiveThread_mtx);
  currentActiveThread = Nothing;
  currentActiveThread_mtx.unlock();
  countIdle = 0;
  log("Exiting afterSleep", emitter);
}

void returnDeepSleep() {
  log("Returning from deep sleep", emitter);
  if (deepSleep == true) {
    remove("/run/ipd/sleepCall");
    setCpuGovernor(cpuGovernorToSet);
  }
  deepSleep = false;
  deepSleepPermission = true;
}
