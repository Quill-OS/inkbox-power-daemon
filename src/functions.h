#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <mutex>
#include <thread>

using namespace std;

void log(string message, string emitter = "undefined");
void waitMutex(mutex* exampleMutex);
void prepareVariables();
string readConfigString(string path);
void writeFileString(string path, string stringToWrite);
string readFile(string path);
bool fileExists(string fileName);
bool dirExists(string path);
string executeCommand(string command);
string normalReplace(string mainString, string strToLookFor, string replacement);
string readConfigStringNoLog(string path);
bool normalContains(string stringToCheck, string stringToLookFor);
bool readConfigBool(string path);
// Create arguments like this:
// const char *args[] = {execPath.c_str(), firstArgument.c_str(), secondArgument.c_str(), nullptr};
// Use real variables for pid to avoid changing an argument, not sure how C++ handles this
void posixSpawnWrapper(string path, const char *args[], bool wait, pid_t* pid);

enum goSleepCondition
{
    None,
    powerButton,
    halSensor,
    Idle,
    Inotify
};

enum sleepBool
{
    Nothing,
    Prepare,
    GoingSleep,
    After,   
    Skip,
};

#endif
