#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "functions.h"
#include "idleSleep.h"
#include "appsFreeze.h"

// libevdev
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libevdev/libevdev.h"

const std::string emitter = "idleSleep";

using namespace std;

extern bool watchdogStartJob;
extern mutex watchdogStartJob_mtx;

extern goSleepCondition newSleepCondition;
extern mutex newSleepCondition_mtx;

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

extern sleepBool watchdogNextStep;

extern int idleSleepTime;
extern int countIdle;

extern string model;

extern bool deviceRooted;


void startIdleSleep() {
  chrono::milliseconds timespan(1000);
  log("Starting idleSleep", emitter);

  log("Waiting for inkbox-bin to start", emitter);
  bool waitForInkbox = true;
  int waitCount = 0;
  while(waitForInkbox == true) {
    if(getPidByName("inkbox-bin") != -1) {
      waitForInkbox = false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    waitCount = waitCount + 1;
    if(waitCount > 90) {
      log("Waited 1.5 minute for inkbox-bin: giving up; starting idle sleep service", emitter);
      break;
    }
  }
  if (waitForInkbox == false) log("inkbox-bin started. Waiting additional 30 seconds", emitter);
  std::this_thread::sleep_for(std::chrono::milliseconds(30000));

  struct libevdev *dev = NULL;

  int fd = -1;
  string path;
  if(model != "kt") {
    path = "/dev/input/event1";
  }
  else {
    path = "/dev/input/event2";
  }

  // Open until it's fine...
  while(fd < 0) {
    fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if(fd > 0) {
      break;
    }
    log("Device file failed to open, waiting " + path, emitter);
    this_thread::sleep_for(timespan);
  }
 
  int rc = libevdev_new_from_fd(fd, &dev);

  if (rc < 0) {
    log("Failed to init libevdev: " + (string)strerror(-rc), emitter);
  } 
  else {
    log("Input device name: " + (string)libevdev_get_name(dev), emitter);
    log("Input device bus: " + to_string(libevdev_get_id_bustype(dev)) +
      " vendor: " + to_string(libevdev_get_id_vendor(dev)) +
      " product: " + to_string(libevdev_get_id_product(dev)), emitter);
  }

  // Add to it every second, and make operations based on this timing
  countIdle = 0;
  struct input_event ev;
  do {
    if (idleSleepTime != 0) {
      if (idleSleepTime == countIdle) {
        countIdle = 0;
        waitMutex(&sleep_mtx);
        // Do absolutely everything to not break things, to not go to idle sleep when other things are going on
        if (sleepJob == Nothing) {
          sleep_mtx.unlock();
          if (watchdogNextStep == Nothing) {
            waitMutex(&currentActiveThread_mtx);
            if (currentActiveThread == Nothing) {
              if(deviceRooted == true) {
                // If the device is rooted, and a SSH connection is active, don't go to sleep and show a message
                // this will execute only if the device is rooted, so don't worry
                if(normalContains(executeCommand("ss | grep -o ssh"), "ssh") == true) {
                  notifySend("Idle sleep: SSH active");
                  log("Skipping idle sleep call because of an active SSH session", emitter);
                  currentActiveThread_mtx.unlock(); // Remember that...
                  continue;
                }
              }
              log("Going to sleep because of idle touch screen", emitter);
              currentActiveThread_mtx.unlock();

              waitMutex(&newSleepCondition_mtx);
              newSleepCondition = Idle;
              newSleepCondition_mtx.unlock();
              
              waitMutex(&watchdogStartJob_mtx);
              watchdogStartJob = true;
              watchdogStartJob_mtx.unlock();
              
            } else {
              currentActiveThread_mtx.unlock();
              log("Not going to sleep because of currentActiveThread", emitter);
            }
          } else {
            log("Not going to sleep because of watchdogNextStep", emitter);
          }
        } else {
          sleep_mtx.unlock();
          log("Not going to sleep because of sleepJob", emitter);
        }
      }
    }

    if (libevdev_has_event_pending(dev) == 1) {
      //log("Received touch input, resetting timer", emitter);
      countIdle = 0;
      while (libevdev_has_event_pending(dev) == 1) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
      }
    }

    this_thread::sleep_for(timespan);
    countIdle = countIdle + 1;
    //log("Count idle time:" + to_string(countIdle), emitter);

    while(fileExists("/kobo/tmp/in_usbms")) {
      log("USB mass storage session is active. Delaying idle sleep for additional 5 minutes", emitter);
      countIdle = -300; // to be sure
      this_thread::sleep_for(timespan);
    }

  } while (rc == 1 || rc == 0 || rc == -EAGAIN);
  log("Error: Monitoring events in idle sleep died unexpectedly, restarting it...", emitter);
  startIdleSleep();
}
