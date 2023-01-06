/*
    InkBox Power Daemon (IPD): InkBox OS power management program
    Copyright (C) 2022-2023 Szybet <https://github.com/Szybet> and Nicolas Mailloux <nicolecrivain@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <pthread.h>
#include <string>
#include <thread>
#include <string.h>

#include "appsFreeze.h"
#include "configUpdate.h"
#include "fbinkFunctions.h"
#include "functions.h"
#include "goingSleep.h"
#include "monitorEvents.h"
#include "pipeHandler.h"
#include "watchdog.h"
#include "idleSleep.h"
#include "devices.h"

const std::string emitter = "main";
extern std::string model;

extern bool logEnabled;
extern int fbfd;

using namespace std;

int main() {
  // Some weird bugs, this is the fix, but I lost the link to the fix
  // TODO: Find link describing fix
  static_cast<void>(pthread_create);
  static_cast<void>(pthread_cancel);

  std::cout << "InkBox Power Daemon starting ..." << std::endl;

  const char * debugEnv = std::getenv("DEBUG");

  if (debugEnv != NULL && strcmp(debugEnv, "true") == 0) {
    logEnabled = true;
    log("Debug mode is enabled", emitter);
    log("Saving logs to /var/log/ipd.log", emitter);
  }

  prepareVariables();
  initFbink();
  startPipeServer();

  thread monitorDev;
  if(model != "kt") {
    monitorDev = thread(startMonitoringDev);
  }
  else {
    monitorDev = thread(startMonitoringDevKT);
  }
  thread watchdogThread(startWatchdog);
  thread watchConfig(startMonitoringConfig);
  thread idleSleep(startIdleSleep);

  // Something is turning off the LED when starting all the GUI (inkbox.sh). This is fine until we want a charger indicator
  std::this_thread::sleep_for(std::chrono::milliseconds(30000));
  // If you want to do shut down the LED from IPD, do this here:
  // setLedState(false);
  ledManagerAccurate();

  // https://stackoverflow.com/questions/7381757/c-terminate-called-without-an-active-exception
  monitorDev.join();
  watchdogThread.join();
  watchConfig.join();
  idleSleep.join();
  
  log("How did this end?", emitter);
  return -1;
}
