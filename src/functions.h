#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <mutex>
#include <thread>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

// https://stackoverflow.com/questions/5947286/how-to-load-linux-kernel-modules-from-c-code
#define deleteModule(name, flags) syscall(__NR_delete_module, name, flags)
#define initModule(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)

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
void notifySend(string message);
void launchLockscreen();
bool stringContainsUSBMSModule(string fileContent);
string repairString(string word);
string removeSpaces(string word);

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
