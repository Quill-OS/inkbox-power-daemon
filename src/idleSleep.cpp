#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "functions.h"
#include "idleSleep.h"

// libevdev
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
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

void startIdleSleep() {
  log("Starting idleSleep", emitter);

  struct libevdev *dev = NULL;

  int fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
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
        log("Going to sleep because of idle time", emitter);
        countIdle = 0;
        waitMutex(&sleep_mtx);
        // Do absolutely everything to not break things, to not go to idle sleep when other things are going on
        if (sleepJob == Nothing) {
          sleep_mtx.unlock();
          if (watchdogNextStep == Nothing) {
            waitMutex(&currentActiveThread_mtx);
            if (currentActiveThread == Nothing) {
              log("Going to sleep because of idle touch screen", emitter);
              currentActiveThread_mtx.unlock();
              
              waitMutex(&watchdogStartJob_mtx);
              watchdogStartJob = true;
              watchdogStartJob_mtx.unlock();

              waitMutex(&newSleepCondition_mtx);
              newSleepCondition = Idle; // TODO: Use this somewhere
              newSleepCondition_mtx.unlock();
            } else {
              currentActiveThread_mtx.unlock();
              log("idleSleep: Not going to sleep because of currentActiveThread", emitter);
            }
          } else {
            log("idleSleep: Not going to sleep because of watchdogNextStep", emitter);
          }
        } else {
          sleep_mtx.unlock();
          log("idleSleep: Not going to sleep because of sleepJob", emitter);
        }
      }
    }

    if (libevdev_has_event_pending(dev) == 1) {
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
