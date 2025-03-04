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

// open, write, close
#include <fcntl.h>
#include <unistd.h>

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

extern bool chargerControllerEnabled;

extern string model;
extern bool isNiaModelA;

bool diePrepare;

// To show warning
extern bool xorgAppRunning;

extern bool deviceRooted;

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
    else {
      sleep_mtx.unlock();
    }
  }
}

void prepareSleep() {
  waitMutex(&occupyLed);
  log("Launching prepareSleep", emitter);
  diePrepare = false;

  if(isNiaModelA == true) {
    if(getPidByName("luajit") == -1) {
      niaATouchscreenLoader(false);
    } else {
      log("Not putting touchscreen off because of koreader and lua friends", emitter);
    }
  }

  CEP();
  if (diePrepare == false) {
    // To avoid screen shooting the previous lockscreen
    // But if the lockscreen is the boot lockscreen, then screen shoot it
    // In the future if more things call lockscreen, we could use lockscreen pid to check if it's from ipd
    if(getPidByName("lockscreen-bin") == -1 || getPidByName("inkbox.sh") != -1) {
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
    // killProcess doesn't need checking, it does it internally
    if(getPidByName("inkbox.sh") == -1) {
      killProcess("launch_lockscreen.sh");
      killProcess("lockscreen-bin");
      killProcess("lockscreen");
    }
    if(isNiaModelA == true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    freezeApps();
  }

  CEP();
  if (diePrepare == false) {
    saveFbDepth();
    clearScreen(darkMode);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    sleepScreen();
  }

  CEP();
  if (diePrepare == false) {
    if(xorgAppRunning == true) {
      // Experimental
      notifySend("KoBox suspend is experimental");
    }
  }

  CEP();
  if (diePrepare == false) {
    writeFileString("/tmp/sleep_standby", "true");
    writeFileString("/tmp/sleep_mode", "true");
  }

  CEP();
  if (diePrepare == false) {
    saveBrightness(getBrightness(0), 0);
    saveBrightness(getBrightness(1), 1);
    setBrightnessCin(0, 0, getBrightness(0), getBrightness(1));
  }

  // kill inactive ssh sessions
  // ps -ef | grep sshd | grep -v -e grep -e /usr/sbin/sshd | awk '{print $1}' | xargs kill -9 &
  CEP();
  if (diePrepare == false) {
    if(deviceRooted) {
      executeCommand("ps -ef | grep sshd | grep -v -e grep -e /usr/sbin/sshd | awk '{print $1}' | xargs kill -9 &");
    }
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

  // Charger controller - revert to gadget
  // Keeping this in here instead of devices.cpp until more devices are added
  CEP();
  if (diePrepare == false) {
    if(model == "n306") {
      if(chargerControllerEnabled == true) {
        log("Setting gadget as USB mode", emitter);
        string strToWrite = "gadget";
        int dev = open("/sys/kernel/debug/ci_hdrc.0/role", O_RDWR);
        int writeStatus = write(dev, strToWrite.c_str(), strToWrite.length());
        close(dev);
      }
    }
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
  const string screenSaverPath = "/data/onboard/.screensaver";
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
