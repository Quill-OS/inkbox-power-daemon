#include "fbinkFunctions.h"
#include "functions.h"

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include "fbink.h"

const std::string emitter = "fbinkFunctions";
extern int fbfd;
extern FBInkDump dump;
extern string model;

// Because of lockscreen at boot it sometimes isn't taken, so avoid dumping random bytes
// Default value here is very important
bool dumpSuccess = false;

void initFbink() {
  fbfd = fbink_open();
  if (fbfd == -1) {
    log("Failed to open the framebuffer", emitter);
    //exit(EXIT_FAILURE);
  }
  log("Loaded FBInk version: " + (string)fbink_version(), emitter);
}

int fbinkRefreshScreen() {
  log("FBInk: Full screen refresh", emitter);
  FBInkConfig fbink_cfg = {0};
  fbink_cfg.is_flashing = true;
  int rv = fbink_cls(fbfd, &fbink_cfg, NULL, false);
  fbink_wait_for_complete(fbfd, LAST_MARKER);
  return rv;
}

int fbinkWriteCenter(string stringToWrite, bool darkMode) {

  FBInkConfig fbink_cfg = {0};
  fbink_cfg.is_centered = true;
  fbink_cfg.is_halfway = true;
  if (darkMode == true) {
    fbink_cfg.is_inverted = true;
  }

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk, aborting", emitter);
    return EXIT_FAILURE;
  }

  fbink_add_ot_font("/etc/init.d/splash.d/fonts/resources/inter-b.ttf", FNT_REGULAR);

  FBInkOTConfig fbinkOT_cfg = {0};
  fbinkOT_cfg.is_centered = true;
  fbinkOT_cfg.is_formatted = true;
  fbinkOT_cfg.size_pt = 20;

  FBInkOTFit ot_fit = {0};

  if (fbink_print_ot(fbfd, stringToWrite.c_str(), &fbinkOT_cfg, &fbink_cfg, &ot_fit) < 0) {
    log("FBInk: Failed to print string", emitter);
  }
  fbink_wait_for_complete(fbfd, LAST_MARKER);
  
  return EXIT_SUCCESS;
}

void clearScreen(bool darkModeSet) {
  FBInkConfig fbink_cfg = {0};
  fbink_cfg.is_inverted = darkModeSet;

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk, aborting", emitter);
    //exit(EXIT_FAILURE);
  }

  FBInkRect cls_rect = {0};
  cls_rect.left = (unsigned short int)0;
  cls_rect.top = (unsigned short int)0;
  cls_rect.width = (unsigned short int)0;
  cls_rect.height = (unsigned short int)0;
  fbink_cls(fbfd, &fbink_cfg, &cls_rect, false);
  fbink_wait_for_complete(fbfd, LAST_MARKER);
}

int printImage(string path) {
  FBInkConfig fbink_cfg = {0};

  // Resize the image ( + adjust for rotation too )
  // fbink is awesome
  fbink_cfg.scaled_width = -1;
  fbink_cfg.scaled_height = -1;

  int status = fbink_print_image(fbfd, path.c_str(), 0, 0, &fbink_cfg);
  fbink_wait_for_complete(fbfd, LAST_MARKER);

  return status;
}

void screenshotFbink() {
  FBInkConfig fbink_cfg = {0};

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk in screenshotFbink", emitter);
  } else {
    log("FBInk was initialized successfully in screenshotFbink", emitter);
  }

  if (fbink_dump(fbfd, &dump) < 0) {
    log("FBInk: Something went wrong while trying to dump the screen", emitter);
    dumpSuccess = false;
  } 
  else {
    dumpSuccess = true;
  }
  log("Screen dump done", emitter);
}

void restoreFbink(bool darkMode) {
  if(dumpSuccess == false) {
    log("Variable dumpSuccess is false, avoid taking dump", emitter);
    return void();
  }
  FBInkConfig fbink_cfg = {0};
  if (darkMode == true) {
    fbink_cfg.is_nightmode = true;
  }

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk in restoreFbink, aborting", emitter);
  }
  else {
    fbinkRefreshScreen();
    fbink_wait_for_complete(fbfd, LAST_MARKER);
    fbink_restore(fbfd, &fbink_cfg, &dump);
    fbink_wait_for_complete(fbfd, LAST_MARKER);
  }
}

void closeFbink() { fbink_close(fbfd); }

int fbDepthSaved = -1;
void restoreFbDepth() {
  /*
  // Not tested
  // https://github.com/NiLuJe/FBInk/blob/9c1be635137ee5d8db9f8d5516aad1c818abc31d/fbink.h#L1287
  const FBInkConfig fbink_cfg = {0};
  if (model == "n437" or model == "kt" or model == "n306") {
    if (fileExists("/var/run/openrc/started/inkbox_gui") == true) {
      // Set bitdepth to 8 BPP
      if(fbink_set_fb_info(fbfd, KEEP_CURRENT_ROTATE, 8, KEEP_CURRENT_GRAYSCALE, &fbink_cfg) < 0) {
        log("FBInk: Something went wrong when trying to change screen bitdepth to 8 bpp", emitter);
      }
    } else {
      // X11 is running, elsewise there is something wrong ...
      if (model == "kt") {
        // Set bitdepth to 32 BPP
        if(fbink_set_fb_info(fbfd, KEEP_CURRENT_ROTATE, 32, KEEP_CURRENT_GRAYSCALE, &fbink_cfg) < 0) {
          log("FBInk: Something went wrong when trying to change screen bitdepth to 32 bpp", emitter);
	      }
      } else {
        // Set bitdepth to 16 BPP
        if(fbink_set_fb_info(fbfd, KEEP_CURRENT_ROTATE, 16, KEEP_CURRENT_GRAYSCALE, &fbink_cfg) < 0) {
          log("FBInk: Something went wrong when trying to change screen bitdepth to 16 bpp", emitter);
        }
      }
    }
  }
  */
  if(fbDepthSaved > 0) {
    const FBInkConfig fbink_cfg = {0};
    if(fbink_set_fb_info(fbfd, KEEP_CURRENT_ROTATE, fbDepthSaved, KEEP_CURRENT_GRAYSCALE, &fbink_cfg) < 0) {
      log("FBInk: Something went wrong when trying to change screen bitdepth to " + to_string(fbDepthSaved) + " bpp", emitter);
    }
    else {
      log("Restoring fbdepth to " + to_string(fbDepthSaved) + " bpp was succesfull", emitter);
    }
  }
  else {
    log("fbDepthSaved save propably failed", emitter);
  }
}

void saveFbDepth() {
  fbDepthSaved = stoi(executeCommand("/usr/bin/fbdepth -g"));
}
