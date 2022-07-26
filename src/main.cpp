#include <iostream>
#include <pthread.h>
#include <string>
#include <thread>
#include <string.h>

#include "AppsFreeze.h"
#include "configUpdate.h"
#include "fbinkFunctions.h"
#include "functions.h"
#include "goingSleep.h"
#include "monitorEvents.h"
#include "pipeHandler.h"
#include "watchdog.h"
#include "idleSleep.h"

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
    log("Debug mode is activated");
    log("Saving logs to /tmp/PowerDaemonLogs.txt");
  }

  prepareVariables();
  initFbink();
  startPipeServer();

  thread monitorDev(startMonitoringDev);
  thread watchdogThread(startWatchdog);
  thread watchConfig(startMonitoringConfig);
  thread idleSleep(startIdleSleep);

  // https://stackoverflow.com/questions/7381757/c-terminate-called-without-an-active-exception
  monitorDev.join();
  watchdogThread.join();
  watchConfig.join();
  idleSleep.join();
  
  log("How did this end?");
  return -1;
}
