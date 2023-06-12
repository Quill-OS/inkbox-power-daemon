#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <experimental/filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <regex>
#include <signal.h>
#include <sys/wait.h>
#include <spawn.h>

#include "devices.h"
#include "fbink.h"
#include "functions.h"

using namespace std;

// Variables (there is no risk that they will be read at the same time by many threads). Used by: FBInk, internal things
const std::string emitter = "functions";
bool logEnabled = false;
string model;
int fbfd;
FBInkDump dump;
vector<int> appsPids;
bool wasUsbNetOn;

// Configuration variables
int cinematicBrightnessDelayMs;
bool lockscreen;
string lockscreenBackgroundMode = "";
bool darkMode;
string cpuGovernorToSet;
bool whenChargerSleep;
bool chargerWakeUp;
bool reconnectWifi;
bool ledUsage;
int idleSleepTime;

bool customCase;
// When 2 it resets to 0 and triggers the watchdog, so 1 is ignored
int customCaseCount = 0;

bool chargerControllerEnabled;
string chargerControllerPath;

bool deepSleep;
bool deepSleepPermission = true; // Because inotify is weird, if false it will ignore the call. It's called true in afterSleep

bool deviceRooted = false;

// Internal variables used by watchdog and threads
// TODO: Find out if this needs a mutex or not
sleepBool watchdogNextStep = Nothing;

// Mutex variables
bool watchdogStartJob = false;
mutex watchdogStartJob_mtx;

// Use this before watchdogStartJob. Always
goSleepCondition newSleepCondition = None;
mutex newSleepCondition_mtx;

sleepBool sleepJob = Nothing;
mutex sleep_mtx;
sleepBool currentActiveThread = Nothing;
mutex currentActiveThread_mtx;
// To avoid confusion with the LED control (if this is locked, then it other things are done besides charging indicator, like flickering)
mutex occupyLed;

// Avoid writing to the LED if not needed
bool ledState = false;
// Count for idle
int countIdle = 0;
// To avoid checking this every LED change
string ledPath = "none";
// To collect zombie later
pid_t connectToWifiPid = 0;
pid_t lockscreenPid = 0;

// for blank
bool xorgRunning = false;
bool blankNeeded = false;

// Functions
void log(string toLog, string emitter) {
  if (logEnabled == true) {
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
    std::time_t endTime = std::chrono::system_clock::to_time_t(end);
    std::string message = normalReplace(std::ctime(&endTime), "\n", "\0") + " | " + emitter + ": " + toLog;
    std::cout << message << std::endl;

    // TODO: Improve efficiency (don't close it every time)
    ofstream logFile("/var/log/ipd.log", ios::app);
    logFile << message << std::endl;
    logFile.close();
  }
}

void waitMutex(mutex * exampleMutex) {
  exampleMutex->lock();
}

void prepareVariables() {
  log("Reading system variables", emitter);
  model = readConfigString("/opt/inkbox_device");
  log("Running on: " + model, emitter);

  // Root
  deviceRooted = readConfigBool("/opt/root/rooted");

  // Lockscreen
  string stringRead1 = readConfigString("/opt/config/12-lockscreen/config");
  if (stringRead1 == "true") {
    lockscreen = true;
  } else {
    lockscreen = false;
  }
  log("lockscreen is: " + stringRead1, emitter);

  lockscreenBackgroundMode = normalReplace(readFile("/opt/config/12-lockscreen/background"), "\n", "");
  log("Lockscreen background mode is: " + lockscreenBackgroundMode, emitter);

  // Simply it, for FBInk dump
  dump = {0};

  // Dark mode
  string stringRead2 = readConfigString("/opt/config/10-dark_mode/config");
  if (stringRead2 == "true") {
    log("darkMode is: true", emitter);
    darkMode = true;
  } else {
    log("darkMode is: false", emitter);
    darkMode = false;
  }

  // USB networking
  if (fileExists("/run/openrc/started/usbnet")) {
    log("USB networking system service is running", emitter);
    wasUsbNetOn = true;
  } else {
    log("USB networking system service is not running", emitter);
    wasUsbNetOn = false;
  }

  // Specific daemon configs:
  // TODO: Set it through a configuration file
  // /data/config/20-sleep_daemon
  string mainPath = "/data/config/20-sleep_daemon";
  if (dirExists(mainPath) == false) {
    log("Writing default appsList configuration", emitter);
    experimental::filesystem::create_directory(mainPath);
    // /data/config/20-sleep_daemon/appsList
    writeFileString("/data/config/20-sleep_daemon/appsList",
                    "inkbox-bin\noobe-inkbox-bin\nlockscreen-bin\nqalculate-"
                    "bin\nqreversi-bin\n2048-bin\nscribble");
    writeFileString("/data/config/20-sleep_daemon/updateConfig", "false");
  }

  // 1 - cinematicBrightnessDelayMs
  string cinematicPath = "/data/config/20-sleep_daemon/1-cinematicBrightnessDelayMs";
  if (fileExists(cinematicPath) == true) {
    cinematicBrightnessDelayMs = stoi(normalReplace(readConfigString(cinematicPath), "\n", ""));
  } 
  else {
    writeFileString(cinematicPath, "50");
    cinematicBrightnessDelayMs = 50;
  }

  // 2 - cpuGovernor
  string cpuGovernorPath = "/data/config/20-sleep_daemon/2-cpuGovernor";
  if (fileExists(cpuGovernorPath) == true) {
    cpuGovernorToSet = readConfigString(cpuGovernorPath);
    setCpuGovernor(cpuGovernorToSet);
  } 
  else {
    writeFileString(cpuGovernorPath, "ondemand");
    cpuGovernorToSet = "ondemand";
  }

  // 3 - whenChargerSleep
  string whenChargerSleepPath = "/data/config/20-sleep_daemon/3-whenChargerSleep";
  if (fileExists(whenChargerSleepPath) == true) {
    if(readConfigBool(whenChargerSleepPath) == true)
    {
      whenChargerSleep = true;
    } else {
      whenChargerSleep = false;
    }
  } 
  else {
    // Those devices have problems when going to sleep with a charger
    if (isDeviceChargerBug() == true) {
      writeFileString(whenChargerSleepPath, "false");
      whenChargerSleep = false;
    } else {
      writeFileString(whenChargerSleepPath, "true");
      whenChargerSleep = true;
    }
  }

  // 4 - chargerWakeUp
  string chargerWakeUpPath = "/data/config/20-sleep_daemon/4-chargerWakeUp";
  if (fileExists(chargerWakeUpPath) == true) {
    if(readConfigBool(chargerWakeUpPath) == true) {
      chargerWakeUp = true;
    } 
    else {
      chargerWakeUp = false;
    }
  } 
  else {
    writeFileString(chargerWakeUpPath, "false");
    chargerWakeUp = false;
  }

  // 5 - wifiReconnect
  string reconnectWifiPath = "/data/config/20-sleep_daemon/5-wifiReconnect";
  if (fileExists(reconnectWifiPath) == true) {
    if(readConfigBool(reconnectWifiPath) == true) {
      reconnectWifi = true;
    } 
    else {
      reconnectWifi = false;
    }
  } 
  else {
    writeFileString(reconnectWifiPath, "true");
    reconnectWifi = true;
  }

  // 6 - ledUsage
  string ledUsagePath = "/data/config/20-sleep_daemon/6-ledUsage";
  if (fileExists(ledUsagePath) == true) {
    if(readConfigBool(ledUsagePath) == true) {
      ledUsage = true;
    } 
    else {
      ledUsage = false;
    }
  }
  else {
    writeFileString(ledUsagePath, "false");
    reconnectWifi = false;
  }
  setLedState(false);

  // 7 - idleSleep
  string idleSleepTimePath = "/data/config/20-sleep_daemon/7-idleSleep";
  if (fileExists(idleSleepTimePath) == true) {
    string intToConvert = normalReplace(readConfigString(idleSleepTimePath), "\n", "");
    idleSleepTime = stoi(intToConvert);
  }
  else {
    writeFileString(idleSleepTimePath, "300");
    idleSleepTime = 60;
  }

  // 8 - customCase
  string customCasePath = "/data/config/20-sleep_daemon/8-customCase";
  if (fileExists(customCasePath) == true) {
    if(readConfigBool(customCasePath) == true) {
      customCase = true;
    } 
    else {
      customCase = false;
    }
  } 
  else {
    writeFileString(customCasePath, "false");
    customCase = false;
  }

  // 9 - deepSleep
  string deepSleepPath = "/data/config/20-sleep_daemon/9-deepSleep";
  if (fileExists(deepSleepPath) == false) {
    writeFileString(deepSleepPath, "false");
  }

  // 10 - chargerController
  string chargerControllerPathConfig = "/data/config/20-sleep_daemon/10-chargerController";
  if (fileExists(chargerControllerPathConfig) == true) {
    chargerControllerPath = normalReplace(readConfigString(chargerControllerPathConfig), "\n", "");
    if (fileExists(chargerControllerPath) == true) {
      chargerControllerEnabled = true;
    }
  }
  else {
    chargerControllerEnabled = false;
  }

  // X - Xorg checking
  string xorgFlagPath = "/boot/flags/X11_START";
  if (fileExists(xorgFlagPath) == true) {
    if(readConfigBool(xorgFlagPath) == true) {
      xorgRunning = true;
    }
    else {
      xorgRunning = false;
    }
  }

  // Blank checking for xorg
  if(model == "kt") {
    blankNeeded = true;
  }
  else {
    if(xorgRunning == true) {
      string blankFlag = "/boot/flags/X11_START";
      if(fileExists(blankFlag) == true) {
        if(readConfigBool(xorgFlagPath) == false) {
          if (model == "n306" or model == "n873") {
            blankNeeded = true;
          }
          else {
            blankNeeded = false;
          }
        }
      } 
      else {
        if (model == "n306" or model == "n873") {
          blankNeeded = true;
        }
        else {
          blankNeeded = false;
        }
      }
    }
    else {
      blankNeeded = false;
    }
  }

  if(blankNeeded == true) {
    log("Blank will be used", "functions");
  }

  getLedPath();

  // Handle turning off the LED usage option
  ledManagerAccurate();
}

string readConfigString(string path) {
  ifstream indata;
  string returnData;
  indata.open(path);
  if (!indata) {
    log("Couldn't read config file: " + path, emitter);
    return "none";
  }
  indata >> returnData;
  while (!indata.eof()) {
    indata >> returnData;
  }
  indata.close();
  log(path + " is: " + returnData, emitter);
  return returnData;
}

void writeFileString(string path, string stringToWrite) {
  fstream file;
  file.open(path, ios::out);
  if (!file) {
    log("File could not be created at path: " + path, emitter);
  } 
  else {
    file << stringToWrite;
    file.close();
    log("Wrote: \"" + stringToWrite + "\" to: " + path, emitter);
  }
}

string readFile(string path) {
  ifstream input_file(path);
  if (!input_file.is_open()) {
    log("Could not open file: " + path, emitter);
    return "";
  }
  return string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
}

bool fileExists(string fileName) {
  std::ifstream infile(fileName);
  return infile.good();
}

bool dirExists(string path) {
  struct stat info;

  if (stat(path.c_str(), &info) != 0)
    return false;
  else if (info.st_mode & S_IFDIR)
    return true;
  else
    return false;
}

string executeCommand(string command) {
  char buffer[128];
  string result = "";

  // Open pipe on file
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return "popen failed!";
  }

  // Read until end of process:
  while (!feof(pipe)) {
    // Use buffer to read and add to result
    if (fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }

  pclose(pipe);

  log("Output of command: \"" + command + "\"" + " is: \" " + result + " \"", emitter);
  return result;
}

string normalReplace(string mainString, string strToLookFor, string replacement) {
  return std::regex_replace(mainString, std::regex(strToLookFor), replacement);
}

string readConfigStringNoLog(string path) {
  ifstream indata;
  string returnData;
  indata.open(path);
  if (!indata) {
    log("Couldn't read config file: " + path, emitter);
    return "none";
  }
  indata >> returnData;
  while (!indata.eof()) {
    indata >> returnData;
  }
  indata.close();
  return returnData;
}

bool normalContains(string stringToCheck, string stringToLookFor)
{
  if(stringToCheck.find(stringToLookFor) != std::string::npos) {
    return true;
  } else {
    return false;
  }
}

bool readConfigBool(string path) {
  ifstream indata;
  string stringData;
  indata.open(path);
  if (!indata) {
    log("Couldn't read config file: " + path, emitter);
    return false;
  }
  indata >> stringData;
  while (!indata.eof()) {
    indata >> stringData;
  }
  indata.close();

  // This approach doesn't care about special characters ('\n')
  if(normalContains(stringData, "true") == true) {
    log("Path: " + path + " contains a true boolean", emitter);\
    return true;
  } else {
    log("Path: " + path + " contains a false boolean", emitter);
    return false;
  }
}

void posixSpawnWrapper(string path, const char *args[], bool wait, pid_t* pid) {
  log("Posix spawning " + path, emitter);
  int status = -1;

  status = posix_spawn(pid, path.c_str(), nullptr, nullptr, const_cast<char **>(args), environ);
  if(status == 0) {
    log("Spawning without errors", emitter);
  }
  else {
    log("Error spawning", emitter);
  }
  log("Spawned with PID: " + to_string(*pid), emitter);
  if(wait == true) {
    waitpid(*pid, 0, 0);
  }
}

void notifySend(string message) {
  // Displays a notification on the device's screen via FBInk
  log("notifySend message: " + message, emitter);
  const char *args[] = {"/usr/local/bin/notify-send", message.c_str(), nullptr};
  int fakePid = 0;
  posixSpawnWrapper("/usr/local/bin/notify-send", args, true, &fakePid);
}

/*
// In /usr/local/bin/launch_lockscreen.sh:
#!/bin/sh
chroot /kobo /bin/sh -c "env QT_QPA_PLATFORM=kobo /mnt/onboard/.adds/inkbox/lockscreen; echo exiting; exit 0"
*/

void launchLockscreen() {
  log("Launching lockscreen", emitter);
  const char *args[] = {"/usr/local/bin/launch_lockscreen.sh", nullptr};
  posixSpawnWrapper("/usr/local/bin/launch_lockscreen.sh", args, false, &lockscreenPid);
}
