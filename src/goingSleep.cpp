#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <cerrno>
#include <exception>
#include <experimental/filesystem>
#include <fcntl.h>
#include <iostream>
#include <mtd/mtd-user.h>
#include <sstream>
#include <stdio.h>
#include <streambuf>
#include <string>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <thread>

#include "fbinkFunctions.h"
#include "functions.h"
#include "goingSleep.h"
#include "devices.h"

const std::string emitter = "goingSleep";

// Variables
// 4 - chargerWakeUp
extern bool chargerWakeUp;
bool savedChargerState;

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

extern sleepBool watchdogNextStep;

extern mutex occupyLed;

// There is no way to stop the thread, so use this bool
bool dieGoing;

// Some notes
/*

To be able to go so fast in time without delays, before setting 1 to
state-extended, there needs to be a 0. In other words, if there is a 1 full-time
it won't work. For the Nia it works like that. Adjust it for other devices if it
doesn't work

Code explanation:
This thread doesn't set watchdogNextStep or sleepJob because monitor events send
a signal after waking up, so watchdog knows what to do;

*/

// checkExitGoing
void CEG() {
  if (dieGoing == false) {
    manageChangeLedState();
    waitMutex(&sleep_mtx);
    if (sleepJob != GoingSleep) {
      sleep_mtx.unlock();
      log("Terminating goSleep", emitter);
      dieGoing = true;
    }
    sleep_mtx.unlock();
  }
}

void goSleep() {
  log("Started goSleep", emitter);
  dieGoing = false;
  waitMutex(&occupyLed);

  sync();

  CEG();
  if (dieGoing == false) {
    int fd = open("/sys/power/state-extended", O_RDWR);
    write(fd, "1", 1);
    close(fd);
  }

  smartWait(1000);

  CEG();
  log("Going to sleep now!", emitter);
  smartWait(1000);

  bool continueSleeping = true;
  int count = 0;
  while (continueSleeping == true and dieGoing == false) {
    // 4 - chargerWakeUp. Actually we need this variable anyway to know whether we need to wakeup the device after it or not
    savedChargerState = getChargerStatus();

    // https://linux.die.net/man/3/klogctl
    klogctl(5, NULL, 0);

    // Manage the LED here
    occupyLed.unlock();
    ledManager();
    waitMutex(&occupyLed);

    log("Trying suspend", emitter);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int fd2 = open("/sys/power/state", O_RDWR);
    int status = write(fd2, "mem", 3);
    close(fd2);

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 500

    log("Got back from suspend");

    // Read kernel ring buffer, and then keep only lines containing <3>
    char * logs_data;
    ssize_t len = klogctl(10, NULL, 0);
    logs_data = (char *)malloc(len);
    klogctl(3, logs_data, len);
    vector<string> dmesgErrorsVec;
    boost::split(dmesgErrorsVec, logs_data, boost::is_any_of("\n"), boost::token_compress_on);

    free(logs_data);
    string dmesgErrors;
    for (string line : dmesgErrorsVec) {
      if (line.find("<3>") != std::string::npos) {
        dmesgErrors.append(line);
        dmesgErrors.append("\n");
      }
    }
    dmesgErrorsVec.clear();
    if (status == -1 or dmesgErrors.find("PM: Some devices failed to suspend") != std::string::npos) {
      log("Failed to suspend, kernel ring buffer errors:\n" + dmesgErrors, emitter);
      log("\nstatus of writing to /sys/power/state: " + to_string(status), emitter);
      CEG();
      count = count + 1;
      if (count == 5) {
        log("5 failed attempts at suspending, sleep a little longer ...", emitter);
        smartWait(10000);
      } else if (count == 15) {
        log("15 failed attempts at sleeping ...");
        // TODO: Display an error splash screen with FBInk
      } else {
        smartWait(3000);
      }
    } else {
      // Exiting this sleeping hell
      log("FATAL error: stopping suspend attempts after " + to_string(count) + " failed attempts", emitter);
      continueSleeping = false;

      // 4 - chargerWakeUp
      if (savedChargerState != getChargerStatus()) {
        if (chargerWakeUp == true) {
          log("4 - chargerWakeUp option is enabled, and the charger state is different. Going to sleep one more time", emitter);
          count = 0;
          continueSleeping = true;
        } else {
          // Because the charger doesn't trigger anything in monitorEvents
          log("The device woke up because of a charger, but option '4 - chargerWakeUp' is disabled, so it will continue to wake up", emitter);
        }
      }
    }
  }

  occupyLed.unlock();
  watchdogNextStep = After;
  waitMutex(&currentActiveThread_mtx);
  currentActiveThread = Nothing;
  currentActiveThread_mtx.unlock();
  log("Exiting goSleep", emitter);
}

// Sometimes I regret using such a simple multi-threading approach, but then I remember that it is safe
void smartWait(int timeToWait) {
  int time = timeToWait / 20;
  int count = 0;
  while (count < 20 and dieGoing == false) {
    count = count + 1;
    // Now that I'm thinking, CEG isn't needed that much here, but the LED flickers more, so it's cool
    CEG();
    std::this_thread::sleep_for(std::chrono::milliseconds(time));
  }
}
