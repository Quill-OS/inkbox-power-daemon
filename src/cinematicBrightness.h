#ifndef CINEMATICBRIGHTNESS_H
#define CINEMATICBRIGHTNESS_H

void setBrightnessCin(int levelToSet, int currentLevel, int type);
void saveBrightness(int level, int type);
int restoreBrightness(int type);
void setBrightness(int device, int level, int type);
int getBrightness(int type);

#endif
