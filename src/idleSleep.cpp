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
  log("Starting idleSleep", emitter);

  log("Waiting for inkbox-bin to start", emitter);
  bool waitForInkbox = true;
  while(waitForInkbox == true) {
    if(getPidByName("inkbox-bin") != -1) {
      waitForInkbox = false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  log("inkbox-bin started. Waiting additional 30 seconds", emitter);
  std::this_thread::sleep_for(std::chrono::milliseconds(30000));

  struct libevdev *dev = NULL;

  int fd;
  if(model != "kt") {
    fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
  }
  else {
    fd = open("/dev/input/event2", O_RDONLY | O_NONBLOCK);
  }
  int rc = libevdev_new_from_fd(fd, &dev);

  if (rc < 0) {
    log("Failed to init libevdev: " + (string)strerror(-rc), emitter);
    exit(1);
  }

  log("Input device name: " + (string)libevdev_get_name(dev), emitter);
  log("Input device bus: " + to_string(libevdev_get_id_bustype(dev)) +
      " vendor: " + to_string(libevdev_get_id_vendor(dev)) +
      " product: " + to_string(libevdev_get_id_product(dev)), emitter);

  chrono::milliseconds timespan(1000);
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
                  log("Skipping idle sleep call because of an active SSH session", emitter);
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
      // log("Received touch input, resetting timer", emitter);
      countIdle = 0;
      while (libevdev_has_event_pending(dev) == 1) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
      }
    }

    this_thread::sleep_for(timespan);
    countIdle = countIdle + 1;
  } while (rc == 1 || rc == 0 || rc == -EAGAIN);
  log("Error: Monitoring events in idle sleep died unexpectedly", emitter);
}
