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

#include "devices.h"
#include "fbink.h"
#include "functions.h"

using namespace std;

// Variables (there is no risk that they will be read at the same time by many threads). Used by: FBInk, internal things

bool logEnabled = false;
string model;
int fbfd;
FBInkDump dump;
vector<int> appsPids;
bool wasUsbNetOn;

// Config var
int cinematicBrightnessDelayMs;
bool lockscreen;
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

bool deepSleep;
bool deepSleepPermission = true; // Because inotify is weird, if false it will ignore the call. It's called true in afterSleep

// Internal variables used by watchdog and threads
// TODO: Find out if this needs a mutex or not
sleepBool watchdogNextStep = Nothing;

// Mutex variables
bool watchdogStartJob = false;
mutex watchdogStartJob_mtx;
goSleepCondition newSleepCondition = None;
mutex newSleepCondition_mtx;
sleepBool sleepJob = Nothing;
mutex sleep_mtx;
sleepBool currentActiveThread;
mutex currentActiveThread_mtx;
// To avoid confusion with the LED control (if this is locked, then it other things are done besides charging indicator, like flickering)
mutex occupyLed;

// Avoid writing to the LED if not needed
bool ledState = false;
// Count for idle
int countIdle = 0;

// Functions
void log(string to_log) {
  if (logEnabled == true) {
    std::cout << to_log << std::endl;

    // TODO: Improve efficiency (don't close it every time)
    ofstream logFile("/var/log/ipd.log", ios::app);
    logFile << to_log << std::endl;
    logFile.close();
  }
}

void waitMutex(mutex * exampleMutex) {
  exampleMutex->lock();
}

void prepareVariables() {
  log("Reading system variables");
  model = readConfigString("/opt/inkbox_device");
  log("Running on: " + model);

  // Lockscreen
  string stringRead1 = readConfigString("/opt/config/12-lockscreen/config");
  if (stringRead1 == "true") {
    lockscreen = true;
  } else {
    lockscreen = false;
  }
  log("lockscreen is: " + stringRead1);

  // Simply it, for FBInk dump
  dump = {0};

  // dark mode
  string stringRead2 = readConfigString("/opt/config/10-dark_mode/config");
  if (stringRead2 == "true") {
    log("darkMode is: true");
    darkMode = true;
  } else {
    log("darkMode is: false");
    darkMode = false;
  }

  // USB networking
  string commandOutput = executeCommand("service usbnet status");
  if (commandOutput.find("status: started") != std::string::npos) {
    log("USB networking system service is started");
    wasUsbNetOn = true;
  } else {
    log("USB networking system service is not running");
    wasUsbNetOn = false;
  }

  // Specific daemon configs:
  // TODO: Set it through a configuration file
  // /data/config/20-sleep_daemon
  string mainPath = "/data/config/20-sleep_daemon";
  if (dirExists(mainPath) == false) {
    log("Creating basic config");
    experimental::filesystem::create_directory(mainPath);
    // /data/config/20-sleep_daemon/appList.txt
    writeFileString("/data/config/20-sleep_daemon/appList.txt",
                    "inkbox-bin\noobe-inkbox-bin\nlockscreen-bin\ncalculator-"
                    "bin\nqreversi-bin\n2048-bin\nscribble\nlightmaps");
    writeFileString("/data/config/20-sleep_daemon/updateConfig", "false");
  }

  // 1 - cinematicBrightnessDelayMs
  string cinematicPath = "/data/config/20-sleep_daemon/1-cinematicBrightnessDelayMs";
  if (fileExists(cinematicPath) == true) {
    cinematicBrightnessDelayMs = stoi(readConfigString(cinematicPath));
  } else {
    writeFileString(cinematicPath, "50");
    cinematicBrightnessDelayMs = 50;
  }

  // 2 - cpuGovernor
  string cpuGovernorPath = "/data/config/20-sleep_daemon/2-cpuGovernor";
  if (fileExists(cpuGovernorPath) == true) {
    cpuGovernorToSet = readConfigString(cpuGovernorPath);
    setCpuGovernor(cpuGovernorToSet);
  } else {
    writeFileString(cpuGovernorPath, "ondemand");
    cpuGovernorToSet = "ondemand";
  }

  // 3 - whenChargerSleep
  string whenChargerSleepPath = "/data/config/20-sleep_daemon/3-whenChargerSleep";
  if (fileExists(whenChargerSleepPath) == true) {
    string boolToConvert = readConfigString(whenChargerSleepPath);
    if (boolToConvert == "true") {
      whenChargerSleep = true;
    } else {
      whenChargerSleep = false;
    }
  } else {
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
    // TODO: Write a function for this
    string boolToConvert = readConfigString(chargerWakeUpPath);
    if (boolToConvert == "true") {
      chargerWakeUp = true;
    } else {
      chargerWakeUp = false;
    }
  } else {
    writeFileString(chargerWakeUpPath, "false");
    chargerWakeUp = false;
  }

  // 5 - wifiReconnect
  string reconnectWifiPath = "/data/config/20-sleep_daemon/5-wifiReconnect";
  if (fileExists(reconnectWifiPath) == true) {
    string boolToConvert = readConfigString(reconnectWifiPath);
    if (boolToConvert == "true") {
      reconnectWifi = true;
    } else {
      reconnectWifi = false;
    }
  } else {
    writeFileString(reconnectWifiPath, "true");
    reconnectWifi = true;
  }

  // 6 - ledUsage
  string ledUsagePath = "/data/config/20-sleep_daemon/6-ledUsage";
  if (fileExists(ledUsagePath) == true) {
    string boolToConvert = readConfigString(ledUsagePath);
    if (boolToConvert == "true") {
      ledUsage = true;
    } else {
      ledUsage = false;
    }
  } else {
    writeFileString(ledUsagePath, "false");
    reconnectWifi = false;
  }
  setLedState(false);

  // 7 - idleSleep
  string idleSleepTimePath = "/data/config/20-sleep_daemon/7-idleSleep";
  if (fileExists(idleSleepTimePath) == true) {
    string intToConvert = readConfigString(idleSleepTimePath);
    intToConvert = normalReplace(intToConvert, "\n", "");
    idleSleepTime = stoi(intToConvert);
  } else {
    writeFileString(idleSleepTimePath, "60");
    idleSleepTime = 60;
  }

  // 8 - customCase
  string customCasePath = "/data/config/20-sleep_daemon/8-customCase";
  if (fileExists(customCasePath) == true) {
    string boolToConvert = readConfigString(customCasePath);
    if (boolToConvert == "true") {
      customCase = true;
    } else {
      customCase = false;
    }
  } else {
    writeFileString(customCasePath, "false");
    customCase = false;
  }

  // 9 - deepSleep
  string deepSleepPath = "/data/config/20-sleep_daemon/9-deepSleep";
  if (fileExists(deepSleepPath) == false) {
    writeFileString(deepSleepPath, "false");
  }
}

string readConfigString(string path) {
  ifstream indata;
  string returnData;
  indata.open(path);
  if (!indata) {
    log("Couldn't read config file: " + path);
    return "none";
  }
  indata >> returnData;
  while (!indata.eof()) {
    indata >> returnData;
  }
  indata.close();
  log(path + " is: " + returnData);
  return returnData;
}

void writeFileString(string path, string stringToWrite) {
  fstream File;
  File.open(path, ios::out);
  if (!File) {
    string message = "File could not be created at path: ";
    message.append(path);
    log(message);
    exit(EXIT_FAILURE);
  } else {
    File << stringToWrite;
    File.close();
    log("Wrote: \"" + stringToWrite + "\" to: " + path);
  }
}

string readFile(string path) {
  ifstream input_file(path);
  if (!input_file.is_open()) {
    string message = "Could not open file: ";
    message.append(path);
    log(message);
    exit(EXIT_FAILURE);
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

  log("Output of command: \" " + command + " \"" + "is: \" " + result + " \"");
  return result;
}

string normalReplace(string MainString, string strToLookFor, string replacement) {
  return std::regex_replace(MainString, std::regex(strToLookFor), replacement);
}

string readConfigStringNoLog(string path) {
  ifstream indata;
  string returnData;
  indata.open(path);
  if (!indata) {
    log("Couldn't read config file: " + path);
    return "none";
  }
  indata >> returnData;
  while (!indata.eof()) {
    indata >> returnData;
  }
  indata.close();
  return returnData;
}
