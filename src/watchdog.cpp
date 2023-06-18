#include <mutex>
#include <string>
#include <thread>

// blank
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

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

extern bool chargerControllerEnabled;
extern string chargerControllerPath;
bool chargerConnected = false; // First boot up the device, then connect the charger, so it will work

// Xorg blank
extern bool blankNeeded;

// TODO: Implement smarter join function

void startWatchdog() {
  std::chrono::milliseconds timespan(200);

  thread prepareThread;
  thread afterThread;
  thread goingThread;

  unsigned long blankArg;
  int blankFd;
  std::uint8_t blankCounter = 5; 
  if(blankNeeded == true) {
    blankFd = open("/dev/fb0", O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    blankArg = VESA_NO_BLANKING;
  }

  while (true) {
    bool saveWatchdogState = false;
    waitMutex(&watchdogStartJob_mtx);
    saveWatchdogState = watchdogStartJob;
    watchdogStartJob = false;
    watchdogStartJob_mtx.unlock();

    // This takes signals from monitorEvents and assigns them to actions
    if (saveWatchdogState == true) {
      log("Watchdog event received", emitter);

      waitMutex(&sleep_mtx);

      // Check if we are in a USB mass storage session; if yes, ignore everything
      if(sleepJob != Skip && fileExists("/kobo/tmp/in_usbms") == true) {
        log("We are in an USBMS session, ignoring all possible sleep calls", emitter);
        sleepJob = Skip;
      }

      // Handling 3 - whenChargerSleep
      // sleepJob checking because of USBMS modules
      if (sleepJob != Skip && whenChargerSleep == false) {
        if (getChargerStatus() == true) {
          log("Skipping watchdog event because of option '3 - whenChargerSleep'", emitter);
          waitMutex(&newSleepCondition_mtx);
          if(newSleepCondition != Idle) {
            notifySend("Can't suspend while charging");
          }
          newSleepCondition_mtx.unlock();
          sleepJob = Skip;
        }
      }

      if (sleepJob == Nothing) {
        log("Launching 'prepare' thread because of 'Nothing' sleep job", emitter);
        // This is here to avoid waiting too long afterwards
        log("Waiting for currentActiveThread_mtx in beginning of prepare launching process", emitter);
        waitMutex(&currentActiveThread_mtx);
        log("Wait finished", emitter);
        sleepJob = Prepare;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            // Possible problem
            log("Waiting for currentActiveThread to become 'Nothing", emitter);
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

        log("Waiting for currentActiveThread_mtx in the middle of prepare launching process", emitter);
        waitMutex(&currentActiveThread_mtx);
        log("Waiting finished", emitter);
        currentActiveThread = Prepare;
        currentActiveThread_mtx.unlock();

        log("Launching thread: Prepare", emitter);
        prepareThread = thread(prepareSleep);
        prepareThread.detach();
      } else if (sleepJob == Prepare) {
        log("Launching 'After' thread because of prepareSleep job", emitter);
        waitMutex(&currentActiveThread_mtx);
        sleepJob = After;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            log("Waiting for currentActiveThread to become 'Nothing'", emitter);
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

        log("Launching thread: 'After'", emitter);
        afterThread = thread(afterSleep);
        afterThread.detach();
      } else if (sleepJob == After) {
        log("Launching 'Prepare' thread because of afterSleep job", emitter);
        waitMutex(&currentActiveThread_mtx);
        sleepJob = Prepare;
        sleep_mtx.unlock();

        if (currentActiveThread != Nothing) {
          bool check = false;
          currentActiveThread_mtx.unlock();
          while (check == false) {
            log("Waiting for currentActiveThread to became nothing", emitter);
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

        log("Launching thread: 'Prepare'", emitter);
        prepareThread = thread(prepareSleep);
        prepareThread.detach();
      } else if (sleepJob == GoingSleep) {
        log("Launching 'After' thread because of goingSleep job", emitter);
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
              log("Waiting for currentActiveThread to become 'Nothing'", emitter);
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

          log("Launching thread: 'After'", emitter);
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

        log("Launching thread: 'After'", emitter);
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

        log("Launching thread: 'goSleep'", emitter);
        goingThread = thread(goSleep);
        goingThread.detach();
      } else {
        exit(EXIT_FAILURE);
      }
      watchdogNextStep = Nothing;
    }

    ledManager();
    
    if(chargerControllerEnabled == true) {
      if(currentActiveThread == Nothing && watchdogNextStep == Nothing && sleepJob == Nothing) {
        bool chargerStatusTmp = getChargerStatus();
        if(chargerConnected != chargerStatusTmp) {
          chargerConnected = chargerStatusTmp;
          if(chargerConnected == true) {
            currentActiveThread_mtx.lock();
            sleep_mtx.lock();
            log("Launching charger controller located at: " + chargerControllerPath, emitter);
            const char *args[] = {chargerControllerPath.c_str(), nullptr};
            int fakePid = 0;
            posixSpawnWrapper(chargerControllerPath, args, true, &fakePid);
          }
          currentActiveThread_mtx.unlock();
          sleep_mtx.unlock();
        }
      }
    }

    // Xorg fix
    if(blankNeeded == true) {
      if(blankCounter > 10) {
        ioctl(blankFd, FBIOBLANK, blankArg);
        // log("Using blank", emitter);
        blankCounter = 0;
      }
      else {
        blankCounter = blankCounter + 1;
      }
    }

    std::this_thread::sleep_for(timespan);
  }

  prepareThread.join();
  afterThread.join();
  goingThread.join();
}
