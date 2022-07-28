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
// TODO: Take a vector as argument
int getPidByName(string taskName) {
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
      log("Found PID of " + taskName + ": " + entry->d_name, emitter);
      // After closing directory, it's impossible to call entry->d_name
      int intToReturn = stoi(entry->d_name);
      closedir(dp);
      return intToReturn;
    }
  }
  log("Couldn't find PID of " + taskName, emitter);
  closedir(dp);
  return EXIT_FAILURE;
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

string getRunningUserApp() {
  return readConfigString("/kobo/tmp/currentlyRunningUserApplication");
}

void freezeApps() {
  vector<int> pidVector;

  vector<string> builtInApps = getBuiltInAppsList("/data/config/20-sleep_daemon/appsList");

  for (string &app : builtInApps) {
    pidVector.push_back(getPidByName(app));
  }
  if (fileExists("/kobo/tmp/currentlyRunningUserApplication") == true) {
    pidVector.push_back(stoi(getRunningUserApp()));
  }

  appsPids = pidVector;

  // SIGCONT - continue
  // SIGSTOP - force freeze
  for (int &pid : appsPids) {
    kill(pid, SIGSTOP);
  }
}

void unfreezeApps() {
  for (int &pid : appsPids) {
    kill(pid, SIGCONT);
  }
}

void killProcess(string name) {
  int pid = getPidByName(name);
  if(pid != -1) {
    kill(pid, 9);
  }
}
