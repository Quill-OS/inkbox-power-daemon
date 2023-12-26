#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
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

// Kindle Touch (KT)
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>
#define MAX_PAYLOAD 1024  /* Maximum payload size */

#include "libevdev/libevdev.h"

const std::string emitter = "monitorEvents";

using namespace std;

extern bool watchdogStartJob;
extern mutex watchdogStartJob_mtx;

extern goSleepCondition newSleepCondition;
extern mutex newSleepCondition_mtx;

extern bool customCase;
extern int customCaseCount;

chrono::milliseconds timespan(200);
chrono::milliseconds afterEventWait(1000);

extern bool isNiaModelC;
extern bool handleNiaInputs;

extern bool ignoreEvents;
extern mutex ignoreEvents_mtx;

void eventHandler() {
  log("Entered eventHandler", emitter);
  ignoreEvents_mtx.lock();
  if(ignoreEvents == false) {
    log("Making watchdogStartJob to true", emitter);
    waitMutex(&watchdogStartJob_mtx);
    watchdogStartJob = true;
    watchdogStartJob_mtx.unlock();
  }
  ignoreEvents_mtx.unlock();
}

void startMonitoringDev() {
  log("Starting monitoring events", emitter);

  string devPath = "/dev/input/event0";
  if(isNiaModelC == true && handleNiaInputs == false) {
    devPath = "/dev/input/by-path/platform-21f8000.i2c-platform-bd71828-pwrkey-event";
    handleNiaInputs = true;
  }

  // Wait for udev to create the device
  while(fileExists(devPath) == false) {
    log("Device file doesn't exist, waiting for: '" + devPath + "'", emitter);
    this_thread::sleep_for(timespan);
  }

  struct libevdev * dev = NULL;

  // Open until it's fine...
  int fd = -1;
  while(fd < 0) {
    fd = open(devPath.c_str(), O_RDONLY | O_CLOEXEC);
    if(fd > 0) {
      break;
    }
    log("Device file failed to open, waiting " + devPath, emitter);
    this_thread::sleep_for(timespan);
  }

  int rc = libevdev_new_from_fd(fd, &dev);
  // Because user apps and X11 apps grab the device for some reason
  if(libevdev_grab(dev, LIBEVDEV_GRAB) == 0) {
    log("Grabbed " + devPath, emitter);
  }
  else {
    log("ERROR: Failed to grab " + devPath, emitter);
  }

  if (rc < 0) {
    log("Failed to init libevdev: " + (string)strerror(-rc), emitter);
  } 
  else {
      log("Input device name: " + (string)libevdev_get_name(dev), emitter);
      log("Input device bus: " + to_string(libevdev_get_id_bustype(dev)) +
      " vendor: " + to_string(libevdev_get_id_vendor(dev)) +
      " product: " + to_string(libevdev_get_id_product(dev)), emitter);
  }

  do {
    struct input_event ev;
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0) {
      string codeName = (string)libevdev_event_code_get_name(ev.type, ev.code);
      log("Input event received, type: " +
          (string)libevdev_event_type_get_name(ev.type) +
          " codename: " + codeName + " value: " + to_string(ev.value), emitter);
      if (codeName == "KEY_POWER" and ev.value == 1) {
        // Watchdog manages this part so we don't go to sleep every time
        log("monitorEvents: Received power button trigger, attempting device suspend, propably", emitter);

        eventHandler();

        waitMutex(&newSleepCondition_mtx);
        newSleepCondition = powerButton;
        newSleepCondition_mtx.unlock();
      } else if (codeName == "KEY_F1" and ev.value == 1) { // For hall sensor (sleepcover)
        if (customCase == true) {
          log("Option '8 - customCase' is true", emitter);
          customCaseCount = customCaseCount + 1;
          log("customCaseCount is: " + to_string(customCaseCount), emitter);
          if (customCaseCount == 1) {
            log("Ignoring hall sensor trigger because of option '8 - customCase'", emitter);
            notifySend("Case on back detected");
          } else if (customCaseCount >= 2) {
            customCaseCount = 0;
            log("Second hall sensor trigger, attempting device suspend", emitter);
            notifySend("Case on front detected");
            
            eventHandler();

            waitMutex(&newSleepCondition_mtx);
            newSleepCondition = halSensor;
            newSleepCondition_mtx.unlock();
          }
        } else {
          log("Option '8 - customCase' is false:", emitter);
          log("monitorEvents: Received hall sensor trigger, attempting device suspend", emitter);

          eventHandler();

          waitMutex(&newSleepCondition_mtx);
          newSleepCondition = halSensor;
          newSleepCondition_mtx.unlock();
        }
      }
      if (codeName == "KEY_F1" and ev.value == 0) {
        if (customCase == true) {
          log("customCaseCount is " + to_string(customCaseCount), emitter);
          // Don't show this message when going back from sleep
          if(customCaseCount != 0) {
            log("Hall sensor trigger", emitter);
            notifySend("Case disconnected");
          }
        }
      }
    }
    this_thread::sleep_for(timespan);
  }
  while (rc == 1 || rc == 0 || rc == -EAGAIN);
  log("Error: Monitoring events function died unexpectedly", emitter);
  close(fd);
  // Possibly do this for KT too
  // Possible, experimental fix for sometimes IPD not responding
  startMonitoringDev();
}

void startMonitoringDevKT() {
    // Partially taken from https://www.mobileread.com/forums/showpost.php?p=2685079&postcount=14
    // Modified Linux Journal article example to use NETLINK_KOBJECT_UEVENT

    log("Starting monitoring power button input events", emitter);

    struct sockaddr_nl srcAddr, destAddr;
    struct nlmsghdr * nlh = NULL;
    struct msghdr msg;
    struct iovec iov;
    int sockFd;

    sockFd = socket(PF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    memset(&srcAddr, 0, sizeof(srcAddr));

    srcAddr.nl_family = AF_NETLINK;
    srcAddr.nl_pid = getpid(); // Self PID
    srcAddr.nl_groups = 1;     // Interested in group 1<<0

    bind(sockFd, (struct sockaddr*)&srcAddr, sizeof(srcAddr));
    memset(&destAddr, 0, sizeof(destAddr));
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

    iov.iov_base = (void *)nlh;
    iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);

    msg.msg_name = (void *)&destAddr;
    msg.msg_namelen = sizeof(destAddr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while(true) {
      /* Read message from kernel */
      recvmsg(sockFd, &msg, 0);
      char * message = (char*)NLMSG_DATA(nlh);
      if(strcmp(message, "virtual/misc/yoshibutton") == 0) {
        // Fix issues caused by `udevadm trigger'
        if(readFile("/run/power_button_cancel") == "true\n") {
          log("Ignoring power button input event as it has been disabled by '/run/power_button_cancel'", emitter);
          remove("/run/power_button_cancel");
        }
        else {
          log("monitorEvents: Received power button trigger, attempting device suspend", emitter);
          eventHandler();

          waitMutex(&newSleepCondition_mtx);
          newSleepCondition = powerButton;
          newSleepCondition_mtx.unlock();

	  this_thread::sleep_for(afterEventWait);
        }
      }
    }
    close(sockFd);
}
