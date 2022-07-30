#include "configUpdate.h"
#include "functions.h"

//

#include <iostream>

// https://developer.ibm.com/tutorials/l-ubuntu-inotify/
#include "sys/inotify.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

// https://pubs.opengroup.org/onlinepubs/009604599/functions/read.html
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

const std::string emitter = "configUpdate";

extern bool watchdogStartJob;
extern mutex watchdogStartJob_mtx;

extern goSleepCondition newSleepCondition;
extern mutex newSleepCondition_mtx;

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

extern bool deepSleep;
extern bool deepSleepPermission;

void startMonitoringConfig() {
  log("Starting inotify monitoring for system configuration updates", emitter);

  int fd;
  int wd;
  char buffer[BUF_LEN];

  fd = inotify_init();
  if (fd < 0) {
    log("inotify_init failed (kernel too old?)", emitter);
  }

  wd = inotify_add_watch(fd, "/data/config/20-sleep_daemon", IN_MODIFY | IN_CREATE | IN_DELETE);

  wd = inotify_add_watch(fd, "/run/ipd", IN_MODIFY | IN_CREATE | IN_DELETE);

  while (true) {
    int length, i = 0;
    length = read(fd, buffer, BUF_LEN);
    log("inotify read up", emitter);

    if (length < 0) {
      log("Failed to read from buffer", emitter);
    }

    // This loop goes through all changes
    while (i < length) {
      log("inotify loop executed", emitter);
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len) {
        if (event->mask & IN_CREATE) {
          string evenNameString = event->name;
          log("inotify: Detected a create event of name: " + evenNameString, emitter);

          if (evenNameString == "updateConfig") {
            checkUpdateFile();
          }
          if (evenNameString == "sleepCall") {
            sleepInotifyCall();
          }
        } else if (event->mask & IN_DELETE) {
          string message = "What are you doing? This file or directory was deleted: ";
          message.append(event->name);
          log(message, emitter);
        } else if (event->mask & IN_MODIFY) {
          string evenNameString = event->name;
          log("inotify: Detected a modification event of name: " + evenNameString, emitter);
          if (evenNameString == "updateConfig") {
            checkUpdateFile();
          }
          if (evenNameString == "sleepCall") {
            sleepInotifyCall();
          }
        }
      }
      i += EVENT_SIZE + event->len;
      
    }
    log("All events read", emitter);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  log("inotify crashed", emitter);
  (void)inotify_rm_watch(fd, wd);
  (void)close(fd);
}

void checkUpdateFile() {
  if (readConfigString("/data/config/20-sleep_daemon/updateConfig") == "true") {
    log("Updating config from request", emitter);
    prepareVariables();
    writeFileString("/data/config/20-sleep_daemon/updateConfig", "false");
  } else {
    log("updateConfig is false, not updating anything", emitter);
  }
}

void sleepInotifyCall() {
  log("sleepInotifyCall called, going to sleep (probably)", emitter);
  if (deepSleepPermission == true) {
    string deepSleepFile = readConfigString("/run/ipd/sleepCall");
    bool go = false;
    if (deepSleepFile == "deepSleep") {
      deepSleep = true;
      go = true;
    } else if (deepSleepFile == "sleep") {
      go = true;
    }
    if (go == true) {
      deepSleepPermission = false;
      log("Going to sleep, trigger: inotify call", emitter);
      currentActiveThread_mtx.unlock();

      waitMutex(&watchdogStartJob_mtx);
      watchdogStartJob = true;
      watchdogStartJob_mtx.unlock();

      waitMutex(&newSleepCondition_mtx);
      newSleepCondition = Inotify;
      newSleepCondition_mtx.unlock();
    }
  }
}
