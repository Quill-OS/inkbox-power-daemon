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
#include <cstdlib> // https://en.cppreference.com/w/cpp/utility/program/getenv

#include "configUpdate.h"
#include "fbinkFunctions.h"
#include "functions.h"
#include "monitorEvents.h"
#include "pipeHandler.h"
#include "watchdog.h"
#include "idleSleep.h"
#include "devices.h"


// Mystery to solve
#include "goingSleep.h"
#include "appsFreeze.h"
/*
 Without these lines, it does this:
	kobo:~# strace /sbin/ipd
	execve("/sbin/ipd", ["/sbin/ipd"], 0x7e8d2e30 / 12 vars /) = 0
	set_tls(0x76f4c388)                     = 0
	set_tid_address(0x76f4cf3c)             = 1955
	brk(NULL)                               = 0x55143000
	brk(0x55145000)                         = 0x55145000
	mmap2(0x55143000, 4096, PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x55143000
	mprotect(0x54c5b000, 4096, PROT_READ)   = 0
	--- SIGSEGV {si_signo=SIGSEGV, si_code=SEGV_MAPERR, si_addr=NULL} ---
	+++ killed by SIGSEGV +++
	Segmentation fault
 Only after second launch, the first one works
*/

const std::string emitter = "main";
extern std::string model;

extern bool logEnabled;
extern int fbfd;
extern bool isNiaModelC;
extern bool isNiaModelA;

using namespace std;

int main() {
  // Some weird bugs, this is the fix, but I lost the link to the fix
  // TODO: Find link describing fix
  // static_cast<void>(pthread_create);
  // static_cast<void>(pthread_cancel);
  // FIX FOR RANDOM SEGMENTATION FAULTS WHICH IS CAUSING EVEN MORE SEGMENTATION FAULTS OH NOOOOOOOO

  std::cout << "InkBox Power Daemon starting ..." << std::endl;

  const char * debugEnv = std::getenv("DEBUG");

  // String to check because of weird segmentation faults I had
  if (debugEnv != NULL) {
    if(normalContains(string(debugEnv), "true") == true ) {
      logEnabled = true;
      log("Debug mode is enabled", emitter);
      log("Saving logs to /var/log/ipd.log", emitter);
    }
  }

  prepareVariables();

  // so load the touchscreen module if needed
  if(isNiaModelA == true) {
    niaATouchscreenLoader(true);
  }

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
  // This is not needed anymore??
  /*
  thread secondMonitorDev;
  if(isNiaModelC == true) {
    log("Launching second monitorDev", emitter);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Lazy non-mutex management - this is really fine
    secondMonitorDev = thread(startMonitoringDev); // We simply launch another one
  }
  */

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
  /*
  if(isNiaModelC == true) {
    secondMonitorDev.join();
  }
  */
  
  log("How did this end?", emitter);
  return -1;
}
