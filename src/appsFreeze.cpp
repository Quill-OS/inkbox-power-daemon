#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <vector>

#include "appsFreeze.h"
#include "functions.h"

const std::string emitter = "appsFreeze";
extern vector<int> appsPids;

extern bool xorgRunning;
extern bool xorgAppRunning;

// https://ofstack.com/C++/9293/linux-gets-pid-based-on-pid-process-name-and-pid-of-c.html
int getPidByName(string taskName) {
  log("Looking for process matching '" + taskName + "' string", emitter);
  struct dirent *entry = nullptr;
  DIR *dp = nullptr;

  string proc = "/proc/";
  dp = opendir(proc.c_str());
  while ((entry = readdir(dp))) {
    // cmdline is more accurate, status is sometimes buggy
    ifstream file(proc + entry->d_name + "/cmdline");
    // No need to call fileExists, to avoid creating another ifstream
    if(file.good() == false) {
      continue;
    }
    string firstLine;
    getline(file, firstLine);
    firstLine = normalReplace(firstLine, "\n", "");
    firstLine = repairString(firstLine);
    // https://stackoverflow.com/questions/2340281/check-if-a-string-contains-a-string-in-c
    if(normalContains(firstLine, taskName) == true) {
      log("Found PID of " + taskName + ": " + entry->d_name + " in line: " + firstLine, emitter);
      // After closing directory, it's impossible to call entry->d_name
      int intToReturn = stoi(entry->d_name);
      closedir(dp);
      return intToReturn;
    }
  }
  log("Couldn't find PID of " + taskName, emitter);
  closedir(dp);
  return -1;
}

// Can't be used with user applications because it targets everything (unshare et al.) which crashes them
// May be useful in the future
// Used by xorg
vector<int> getPidsByNameAll(string taskName) {
  log("Looking for processes matching '" + taskName + "' string", emitter + ":getPidsByNameAll");

  struct dirent *entry = nullptr;
  DIR *dp = nullptr;

  string proc = "/proc/";
  dp = opendir(proc.c_str());

  vector<int> pidsRet;
  while((entry = readdir(dp))) {
    // cmdline is more accurate, status is sometimes buggy
    ifstream file(proc + entry->d_name + "/cmdline");
    if(file.good() == false) {
      continue;
    }
    string firstLine;
    getline(file, firstLine);
    firstLine = normalReplace(firstLine, "\n", "");
    firstLine = repairString(firstLine);
    // https://stackoverflow.com/questions/2340281/check-if-a-string-contains-a-string-in-c
    if(normalContains(firstLine, taskName) == true) {
      log("Found PID of " + taskName + " : " + entry->d_name + " In line: " + firstLine, emitter + ":getPidsByNameAll");
      int appPid = stoi(entry->d_name);
      pidsRet.push_back(appPid);
    }
  }
  closedir(dp);
  return pidsRet;
}

// /data/config/20-sleep_daemon/appsList
vector<string> getBuiltInAppsList(string path) {
  vector<string> vectorToReturn;
  vector<string> vectorToParse;

  string configFile = readFile(path);
  boost::split(vectorToParse, configFile, boost::is_any_of("\n"), boost::token_compress_on);

  for (string &app : vectorToParse) {
    if (app.empty() == false) {
      log("In app vector: " + app, emitter);
      vectorToReturn.push_back(app);
    }
  }
  return vectorToReturn;
}

int getRunningUserApp() {
  string name = readConfigString("/kobo/tmp/currentlyRunningUserApplication");

  // Prioritise .bin files
  int fileBin = getPidByName(name + ".bin");
  if(fileBin != -1) {
    log("Found user application binary file, that's good", emitter);
    return fileBin;
  };
  return getPidByName(name);
}

vector<int> getRunningXorgPrograms() {
  if(xorgRunning) {
    string programNamePath = "/tmp/X_program";
    if(fileExists(programNamePath) == true) {
      string programName = normalReplace(readFile(programNamePath), "\n", "") ;
      vector<int> programs = getPidsByNameAll(programName);
      // For an unknown reason, xorg programs in cmdline have deletes spaces so
      vector<int> programsNoSpace = getPidsByNameAll(removeSpaces(programName));

      programs.insert(programs.end(), programsNoSpace.begin(), programsNoSpace.end());  
      xorgAppRunning = !programs.empty();

      // Also those:
      vector<int> otherXorgPrograms;
      string i3ProcessName = "i3";
      otherXorgPrograms.push_back(getPidByName(removeSpaces(i3ProcessName)));
      otherXorgPrograms.push_back(getPidByName(i3ProcessName));

      string vncProcessName = "x11vnc";
      otherXorgPrograms.push_back(getPidByName(removeSpaces(vncProcessName)));
      otherXorgPrograms.push_back(getPidByName(vncProcessName));

      string fbinkXdamageProcessName = "fbink_xdamage";
      otherXorgPrograms.push_back(getPidByName(removeSpaces(fbinkXdamageProcessName)));
      otherXorgPrograms.push_back(getPidByName(fbinkXdamageProcessName));

      /*
      // This was causing write to /power/state to freeze the thread
      string xProcessName = "X -nocursor";
      otherXorgPrograms.push_back(getPidByName(removeSpaces(xProcessName)));
      otherXorgPrograms.push_back(getPidByName(xProcessName));
      */

      string otherVncProcessName = "vnc-nographic";
      otherXorgPrograms.push_back(getPidByName(removeSpaces(otherVncProcessName)));
      otherXorgPrograms.push_back(getPidByName(otherVncProcessName));

      for (int &pid : otherXorgPrograms) {
        if(pid != -1) {
          programs.push_back(pid);
        }
      }
      return programs;
    }
  }
  xorgAppRunning = false;
  vector<int> returnVec;
  return returnVec;
}

void freezeApps() {
  vector<int> pidVector;

  vector<string> builtInApps = getBuiltInAppsList("/data/config/20-sleep_daemon/appsList");

  for (string &app : builtInApps) {
    int appPid = getPidByName(app);
    if(appPid != -1) {
      pidVector.push_back(appPid);
    }
  }
  if (fileExists("/kobo/tmp/currentlyRunningUserApplication") == true) {
    int userAppPid = getRunningUserApp();
    if(userAppPid != -1) {
      pidVector.push_back(userAppPid);
    }
  }

  vector<int> xorgPrograms = getRunningXorgPrograms();
  for (int &pid : xorgPrograms) {
    log("Xorg program PID: " + to_string(pid), emitter);
  }
  appsPids.insert(appsPids.end(), xorgPrograms.begin(), xorgPrograms.end());  

  // Here append because of fast clicking something could get not unfreezed
  // https://stackoverflow.com/questions/2551775/appending-a-vector-to-a-vector
  appsPids.insert(appsPids.end(), pidVector.begin(), pidVector.end());

  // SIGCONT - continue
  // SIGSTOP - force freeze
  for (int &pid : appsPids) {
    log("Freezing process with PID " + to_string(pid), emitter);
    killLogger(pid, SIGSTOP);
  }
}

void killLogger(int pid, int sig)
{
  log("Killing process with PID " + to_string(pid) + " with signal " + to_string(sig), emitter);
  kill(pid, sig);
}

void unfreezeApps() {
  for (int &pid : appsPids) {
    log("Unfreezing process with PID " + to_string(pid), emitter);
    kill(pid, SIGCONT);
  }
  appsPids.clear();
}

void killProcess(string name) {
  int pid = getPidByName(name);
  if(pid != -1) {
    log("Killing process with PID " + to_string(pid), emitter);
    kill(pid, 9);
  }
}
