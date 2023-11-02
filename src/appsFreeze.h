#ifndef APPS_FREEZE_H
#define APPS_FREEZE_H

#include <vector>
#include <string>

using namespace std;

vector<string> getBuiltInAppsList(string path);
void freezeApps();
vector<int> getRunningUserApp();
int getPidByName(string taskName);
void unfreezeApps();
void unfreezeApp(int pid);
void killProcess(string name);
void killLogger(int pid, int sig);
vector<int> getPidsByNameAll(string taskName);
vector<int> getRunningXorgPrograms();

#endif
