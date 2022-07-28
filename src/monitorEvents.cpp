#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "functions.h"
#include "monitorEvents.h"

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

const std::string emitter = "monitorEvents";

using namespace std;

extern bool watchdogStartJob;
extern mutex watchdogStartJob_mtx;

extern goSleepCondition newSleepCondition;
extern mutex newSleepCondition_mtx;

extern bool customCase;
extern int customCaseCount;

void startMonitoringDev() {
  log("Monitoring events", emitter);

  struct libevdev * dev = NULL;

  int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  int rc = libevdev_new_from_fd(fd, &dev);
  // Because user apps and X11 apps grab the device for some reason
  if(libevdev_grab(dev, LIBEVDEV_GRAB) == 0) {
    log("Grabbed /dev/input/event0", emitter);
  }
  else {
    log("ERROR: Failed to grab /dev/input/event0", emitter);
  }

  if (rc < 0) {
    log("Failed to init libevdev: " + (string)strerror(-rc), emitter);
    exit(1);
  }

  log("Input device name: " + (string)libevdev_get_name(dev), emitter);
  log("Input device bus: " + to_string(libevdev_get_id_bustype(dev)) +
      " vendor: " + to_string(libevdev_get_id_vendor(dev)) +
      " product: " + to_string(libevdev_get_id_product(dev)), emitter);

  chrono::milliseconds timespan(200);
  chrono::milliseconds afterEventWait(1000);
  do {
    struct input_event ev;
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0) {
      string codeName = (string)libevdev_event_code_get_name(ev.type, ev.code);
      log("Input event received, type: " +
          (string)libevdev_event_type_get_name(ev.type) +
          " codename: " + codeName + " value: " + to_string(ev.value), emitter);
      if (codeName == "KEY_POWER" and ev.value == 1) {
        log("monitorEvents: Received power button trigger, attempting device suspend", emitter);

        waitMutex(&watchdogStartJob_mtx);
        watchdogStartJob = true;
        watchdogStartJob_mtx.unlock();

        waitMutex(&newSleepCondition_mtx);
        newSleepCondition = powerButton;
        newSleepCondition_mtx.unlock();

        this_thread::sleep_for(afterEventWait);
      }

      // For hall sensor (sleepcover)
      if (codeName == "KEY_F1" and ev.value == 1) {
        if (customCase == true) {
          log("Option '8 - customCase' is true", emitter);
          customCaseCount = customCaseCount + 1;
          log("customCaseCount is: " + to_string(customCaseCount), emitter);
          if (customCaseCount == 1) {
            log("Ignoring hall sensor trigger because of option '8 - customCase'", emitter);
          } else if (customCaseCount >= 2) {
            customCaseCount = 0;
            log("Second hall sensor trigger, attempting device suspend", emitter);
            waitMutex(&watchdogStartJob_mtx);
            watchdogStartJob = true;
            watchdogStartJob_mtx.unlock();

            waitMutex(&newSleepCondition_mtx);
            newSleepCondition = halSensor;
            newSleepCondition_mtx.unlock();

            this_thread::sleep_for(afterEventWait);
          }

        } else {
          log("Option '8 - customCase' is false:", emitter);
          log("monitorEvents: Received hall sensor trigger, attempting device suspend", emitter);

          waitMutex(&watchdogStartJob_mtx);
          watchdogStartJob = true;
          watchdogStartJob_mtx.unlock();

          waitMutex(&newSleepCondition_mtx);
          newSleepCondition = halSensor;
          newSleepCondition_mtx.unlock();

          this_thread::sleep_for(afterEventWait);
        }
      }
    }

    this_thread::sleep_for(timespan);

  } while (rc == 1 || rc == 0 || rc == -EAGAIN);
  log("Error: Monitoring events function died unexpectedly", emitter);
}
