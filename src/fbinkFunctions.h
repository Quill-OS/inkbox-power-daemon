#ifndef FBINK_FUNCTIONS_H
#define FBINK_FUNCTIONS_H

#include <string>
#include "fbink.h"

using namespace std;

void initFbink();
int fbinkWriteCenter(string stringToWrite, bool darkMode);
void clearScreen(bool darkModeSet);
int printImage(string path);
void screenshotFbink();
void restoreFbink(bool darkMode);
void closeFbink();
void restoreFbDepth();

#endif
