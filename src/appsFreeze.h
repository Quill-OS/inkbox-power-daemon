#ifndef APPS_FREEZE_H
#define APPS_FREEZE_H

#include <vector>
#include <string>

using namespace std;

vector<string> getBuiltInAppsList(string path);
void freezeApps();
string getRunningUserApp();
int getPidByName(string taskName);
void unfreezeApps();
void killProcess(string name);

#endif
