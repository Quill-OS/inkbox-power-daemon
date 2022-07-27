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
#include <thread>
#include <ctime> // For rand bug

const std::string emitter = "prepareSleep";

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern FBInkDump dump;

extern sleepBool watchdogNextStep;

extern bool darkMode;

extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

extern mutex occupyLed;

extern bool deepSleep;

bool diePrepare;

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
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  CEP();
  if (diePrepare == false) {
    sleepPipeSend();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  CEP();
  if (diePrepare == false) {
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
      log("screensaver count is " + to_string(vectorSize));
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
          return;
        }
      }
    }
  }
  log("Custom screensaver not found or errored, displaying normal message", emitter);
  fbinkRefreshScreen();
  fbinkWriteCenter("Sleeping", darkMode);
}

void deepSleepGo() {
  log("Going to deep sleep", emitter);
  setCpuGovernor("powersave");

  // TODO
}
