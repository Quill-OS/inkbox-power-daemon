#include "prepareSleep.h"
#include "appsFreeze.h"
#include "wifi.h"
#include "cinematicBrightness.h"
#include "devices.h"
#include "fbink.h"
#include "fbinkFunctions.h"
#include "functions.h"
#include "pipeHandler.h"
#include "usbnet.h"

#include <cstdlib>
#include <exception>
#include <experimental/filesystem>
#include <iostream>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <ctime> // For rand bug

const std::string emitter = "prepareSleep";

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern FBInkDump dump;

extern sleepBool watchdogNextStep;

extern bool darkMode;
extern bool lockscreen;
extern string lockscreenBackgroundMode;
extern pid_t lockscreenPid;

extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

extern mutex occupyLed;

extern bool deepSleep;

bool diePrepare;

// To show warning
extern bool xorgAppRunning;

// void checkExitPrepare()
void CEP() {
  if (diePrepare == false) {
    manageChangeLedState();
    waitMutex(&sleep_mtx);
    if (sleepJob != Prepare) {
      sleep_mtx.unlock();
      log("Terminating prepareSleep", emitter);
      diePrepare = true;
    }
    sleep_mtx.unlock();
  }
}

void prepareSleep() {
  waitMutex(&occupyLed);
  log("Launching prepareSleep", emitter);
  diePrepare = false;

  CEP();
  if (diePrepare == false) {
    screenshotFbink();
    if(lockscreen == true) {
      string fbgrabPath = "/usr/bin/fbgrab";
      const char *args[] = {fbgrabPath.c_str(), "/tmp/lockscreen.png", nullptr};
      int fakePid = 0;
      posixSpawnWrapper(fbgrabPath.c_str(), args, true, &fakePid);
    }
    else {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }

  // Managing zombies is a bit... problematic
  // If this needs to be used anywhere else too, make a function out od it
  log("lockscreenPid is " + to_string(lockscreenPid), emitter);
  if (lockscreenPid != 0) {
    // Kill children too
    killProcess("lockscreen-bin");
    killProcess("lockscreen");
    killProcess("launch_lockscreen.sh");
    log("Collecting lockscreen zombie", emitter);
    waitpid(lockscreenPid, 0, 0);
    log("Collected lockscreen zombie", emitter);
    lockscreenPid = 0;
  } 
  else {
    log("No need to collect zombie process", emitter);
  }

  CEP();
  if (diePrepare == false) {
    sleepPipeSend();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  CEP();
  if (diePrepare == false) {
    // To prevent weird things from happening
    if(getPidByName("launch_lockscreen.sh") != -1) {
      killProcess("launch_lockscreen.sh");
    }
    if(getPidByName("lockscreen") != -1) {
      killProcess("lockscreen");
    }
    freezeApps();
  }

  CEP();
  if (diePrepare == false) {
    clearScreen(darkMode);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    sleepScreen();
  }

  CEP();
  if (diePrepare == false) {
    if(xorgAppRunning == true) {
      // Xorg is experimental hehe, it's just a warning - I don't trust xorg so leave it
      // + don't make it more latters - it barely fits on the nia
      // Maybe doesn't fit on the mini
      notifySend("Suspending Xorg apps is experimental");
    }
  }

  CEP();
  if (diePrepare == false) {
    writeFileString("/tmp/sleep_standby", "true");
    writeFileString("/tmp/sleep_mode", "true");
  }

  CEP();
  if (diePrepare == false) {
    saveBrightness(getBrightness());
    setBrightnessCin(0, getBrightness());
  }

  CEP();
  if (diePrepare == false) {
    turnOffWifi();
  }

  CEP();
  if (diePrepare == false) {
    writeFileString("/kobo/inkbox/remount", "false");
    string hwclockPath = "/sbin/hwclock";
    const char *args[] = {hwclockPath.c_str(), "--systohc", "-u", nullptr};
    int fakePid = 0;
    posixSpawnWrapper(hwclockPath.c_str(), args, true, &fakePid);
  }

  CEP();
  if (diePrepare == false) {
    disableUsbNet();
  }

  CEP();
  if (diePrepare == false) {
    watchdogNextStep = GoingSleep;
  }

  CEP();
  if (diePrepare == false) {
    if (deepSleep == true) {
      deepSleepGo();
    }
  }

  // Yes, this will always be executed
  occupyLed.unlock();

  waitMutex(&currentActiveThread_mtx);
  currentActiveThread = Nothing;
  currentActiveThread_mtx.unlock();
  log("Exiting prepareSleep", emitter);
}

// Show a splash with the text 'Sleeping', but also allow custom screensavers
// Writing 'Sleeping' anyway with background
// TODO: Don't write 'Sleeping' when custom screensaver is active
void sleepScreen() {
  string screenSaverPath = "/data/onboard/.screensaver";
  if (dirExists(screenSaverPath) == true) {
    vector<string> imageList;
    for (const auto &entry : experimental::filesystem::directory_iterator(screenSaverPath)) {
      if (string(entry.path()).find(".png") != std::string::npos) {
        imageList.push_back(string(entry.path()));
        log("Found screensaver image: " + string(entry.path()), emitter);
      }
    }
    if (imageList.empty() == false) {
      int vectorSize = imageList.size();
      log("Screensaver count is " + to_string(vectorSize));
      srand(time(0));
      int randomInt = rand() % vectorSize;
      log("Choosing screensaver at: " + to_string(randomInt));
      string chosenScreensaver = imageList.at(randomInt);
      if (fileExists(chosenScreensaver) == true) {
        log("Displaying image with FBInk: " + chosenScreensaver, emitter);
        int status = printImage(chosenScreensaver);
        if (status < 0) {
          log("Error: Failed to display the screensaver image (is it really a picture?)", emitter);
        } 
        else {
          if(lockscreenBackgroundMode == "screensaver") {
            writeFileString("/tmp/screensaver-used.txt", chosenScreensaver);
          }
          return void();
        }
      }
    }
  }
  log("Custom screensaver not found or an error occured; displaying normal message", emitter);
  fbinkRefreshScreen();
  fbinkWriteCenter("Sleeping", darkMode);
}

void deepSleepGo() {
  log("Going to deep sleep", emitter);
  setCpuGovernor("powersave");

  // TODO
}
