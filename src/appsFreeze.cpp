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

// https://ofstack.com/C++/9293/linux-gets-pid-based-on-pid-process-name-and-pid-of-c.html
int getPidByName(string taskName) {
  log("Looking for process named like " + taskName, emitter);
  struct dirent *entry = nullptr;
  DIR *dp = nullptr;

  string proc = "/proc/";
  dp = opendir(proc.c_str());
  while ((entry = readdir(dp))) {
    // cmdline is more accurate, status sometimes is buggy?
    ifstream file(proc + entry->d_name + "/cmdline");
    string firstLine;
    getline(file, firstLine);
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

// Can't be used with user apps because it targets everything - unshare etc which crashes it
// Maybe usefull in the future
vector<int> getPidsByNameAll(string taskName) {
  log("Looking for processes named like " + taskName, emitter + ":getPidsByNameAll");

  struct dirent *entry = nullptr;
  DIR *dp = nullptr;

  string proc = "/proc/";
  dp = opendir(proc.c_str());

  vector<int> pidsRet;
  while ((entry = readdir(dp))) {
    // cmdline is more accurate, status sometimes is buggy?
    ifstream file(proc + entry->d_name + "/cmdline");
    string firstLine;
    getline(file, firstLine);
    // https://stackoverflow.com/questions/2340281/check-if-a-string-contains-a-string-in-c
    if(normalContains(firstLine, taskName) == true) {
      log("Found PID of " + taskName + ": " + entry->d_name + " In line: " + firstLine, emitter + ":getPidsByNameAll");
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
    log("Found user app bin file, that's good", emitter);
    return fileBin;
  };
  return getPidByName(name);
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
