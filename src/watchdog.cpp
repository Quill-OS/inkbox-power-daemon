#include <mutex>
#include <string>
#include <thread>

#include "afterSleep.h"
#include "functions.h"
#include "goingSleep.h"
#include "monitorEvents.h"
#include "prepareSleep.h"
#include "devices.h"

const std::string emitter = "watchdog";

using namespace std;

// Variables
extern bool whenChargerSleep;

extern bool watchdogStartJob;
extern mutex watchdogStartJob_mtx;

extern goSleepCondition newSleepCondition;
extern mutex newSleepCondition_mtx;

extern sleepBool sleepJob;
extern mutex sleep_mtx;

extern sleepBool watchdogNextStep;

// This variable tells us what thread is currently active, to know which to kill
extern sleepBool currentActiveThread;
extern mutex currentActiveThread_mtx;

// TODO: Implement smarter join function

void startWatchdog() {
  std::chrono::milliseconds timespan(200);

  thread prepareThread;
  thread afterThread;
  thread goingThread;

  while (true) {
    bool saveWatchdogState = false;
    waitMutex(&watchdogStartJob_mtx);
    saveWatchdogState = watchdogStartJob;
    watchdogStartJob = false;
    watchdogStartJob_mtx.unlock();

    // This takes signals from monitorEvents and assigns them to actions
    if (saveWatchdogState == true) {
      log("Watchdog event received", emitter);

      // Handling 3 - whenChargerSleep
      if (whenChargerSleep == false) {
        if (getChargerStatus() == true) {
          log("Skipping watchdog event because of option '3 - whenChargerSleep'", emitter);
          sleepJob = Skip;
        }
      }

      waitMutex(&sleep_mtx);

      if (sleepJob == Nothing) {
        log("Launching 'prepare' thread because of 'Nothing' sleep job", emitter);
        // This is here to avoid waiting too long afterwards
        waitMutex(&currentActiveThread_mtx);
        sleepJob = Prepare;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waitMutex(&currentActiveThread_mtx);
            if (currentActiveThread == Nothing) {
              check = true;
            }
            currentActiveThread_mtx.unlock();
          }
        } else {
          currentActiveThread_mtx.unlock();
        }

        waitMutex(&currentActiveThread_mtx);
        currentActiveThread = Prepare;
        currentActiveThread_mtx.unlock();
        prepareThread = thread(prepareSleep);
        prepareThread.detach();
      } else if (sleepJob == Prepare) {
        log("Launching 'after' thread because of prepareSleep job", emitter);
        waitMutex(&currentActiveThread_mtx);
        sleepJob = After;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waitMutex(&currentActiveThread_mtx);
            if (currentActiveThread == Nothing) {
              check = true;
            }
            currentActiveThread_mtx.unlock();
          }
        } else {
          currentActiveThread_mtx.unlock();
        }

        waitMutex(&currentActiveThread_mtx);
        currentActiveThread = After;
        currentActiveThread_mtx.unlock();

        afterThread = thread(afterSleep);
        afterThread.detach();
      } else if (sleepJob == After) {
        log("Launching 'prepare' thread because of afterSleep job", emitter);
        waitMutex(&currentActiveThread_mtx);
        sleepJob = Prepare;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            waitMutex(&currentActiveThread_mtx);
            if (currentActiveThread == Nothing) {
              check = true;
            }
            currentActiveThread_mtx.unlock();
          }
        } else {
          currentActiveThread_mtx.unlock();
        }

        waitMutex(&currentActiveThread_mtx);
        currentActiveThread = Prepare;
        currentActiveThread_mtx.unlock();

        prepareThread = thread(prepareSleep);
        prepareThread.detach();
      } else if (sleepJob == GoingSleep) {
        log("Launching 'after' thread because of goingSleep job", emitter);
        // To be sure a new thread isn't launching anyway
        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        if (watchdogNextStep != After) {
          waitMutex(&currentActiveThread_mtx);
          sleepJob = After;
          sleep_mtx.unlock();

          if (currentActiveThread != Nothing) {
            bool check = false;
            currentActiveThread_mtx.unlock();
            while (check == false) {
              std::this_thread::sleep_for(std::chrono::milliseconds(50));
              waitMutex(&currentActiveThread_mtx);
              if (currentActiveThread == Nothing) {
                check = true;
              }
              currentActiveThread_mtx.unlock();
            }
          } else {
            currentActiveThread_mtx.unlock();
          }

          waitMutex(&currentActiveThread_mtx);
          currentActiveThread = After;
          currentActiveThread_mtx.unlock();

          afterThread = thread(afterSleep);
          afterThread.detach();
        } else {
          log("Event from monitorEvents requested after thread, but watchdogNextStep already wanted it: skipping monitorEvents request", emitter);
          sleep_mtx.unlock();
        }
      } else {
        log("sleepJob is something else (propably 'Skip'). Changing it to 'Nothing'", emitter);
        sleepJob = Nothing;
        sleep_mtx.unlock();
      }
    }
    if (watchdogNextStep != Nothing) {
      log("Launching watchdogNextStep request", emitter);
      // Make sure all jobs exit. they propably already are, because they called it
      waitMutex(&sleep_mtx);
      sleepJob = Nothing;
      sleep_mtx.unlock();

      waitMutex(&currentActiveThread_mtx);
      if (currentActiveThread != Nothing) {
        bool check = false;
        currentActiveThread_mtx.unlock();
        while (check == false) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          waitMutex(&currentActiveThread_mtx);
          if (currentActiveThread == Nothing) {
            check = true;
          }
          currentActiveThread_mtx.unlock();
        }
      } else {
        currentActiveThread_mtx.unlock();
      }

      if (watchdogNextStep == After) {
        log("Launching afterSleep thread from request by watchdogNextStep", emitter);
        waitMutex(&sleep_mtx);
        sleepJob = After;
        sleep_mtx.unlock();

        waitMutex(&currentActiveThread_mtx);
        currentActiveThread = After;
        currentActiveThread_mtx.unlock();

        afterThread = thread(afterSleep);
        afterThread.detach();
      } else if (watchdogNextStep == GoingSleep) {
        log("Launching goingSleep thread from request by watchdogNextStep", emitter);
        waitMutex(&sleep_mtx);
        sleepJob = GoingSleep;
        sleep_mtx.unlock();

        waitMutex(&currentActiveThread_mtx);
        currentActiveThread = GoingSleep;
        currentActiveThread_mtx.unlock();

        goingThread = thread(goSleep);
        goingThread.detach();
      } else {
        exit(EXIT_FAILURE);
      }
      watchdogNextStep = Nothing;
    }
    ledManager();
    std::this_thread::sleep_for(timespan);
  }

  prepareThread.join();
  afterThread.join();
  goingThread.join();
}
