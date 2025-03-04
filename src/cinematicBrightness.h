#ifndef CINEMATICBRIGHTNESS_H
#define CINEMATICBRIGHTNESS_H

void setBrightnessCin(int levelToSetCold, int levelToSetWarm, int currentLevelCold, int currentLevelWarm);
void saveBrightness(int level, int type);
int restoreBrightness(int type);
void setBrightness(int device, int level, int type);
int getBrightness(int type);

#endif
