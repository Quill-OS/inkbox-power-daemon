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

void initFbink() {
  fbfd = fbink_open();
  if (fbfd == -1) {
    log("Failed to open the framebuffer", emitter);
    exit(EXIT_FAILURE);
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

  return EXIT_SUCCESS;
}

void clearScreen(bool darkModeSet) {
  FBInkConfig fbink_cfg = {0};
  fbink_cfg.is_inverted = darkModeSet;

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk, aborting", emitter);
    exit(EXIT_FAILURE);
  }

  FBInkRect cls_rect = {0};
  cls_rect.left = (unsigned short int)0;
  cls_rect.top = (unsigned short int)0;
  cls_rect.width = (unsigned short int)0;
  cls_rect.height = (unsigned short int)0;
  fbink_cls(fbfd, &fbink_cfg, &cls_rect, false);
}

int printImage(string path) {
  FBInkConfig fbink_cfg = {0};

  return fbink_print_image(fbfd, path.c_str(), 0, 0, &fbink_cfg);
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
  };
  log("Screen dump done", emitter);
}

void restoreFbink(bool darkMode) {
  FBInkConfig fbink_cfg = {0};
  if (darkMode == true) {
    fbink_cfg.is_nightmode = true;
  }

  if (fbink_init(fbfd, &fbink_cfg) < 0) {
    log("Failed to initialize FBInk in restoreFbink, aborting"), emitter;
  }

  fbink_restore(fbfd, &fbink_cfg, &dump);
  fbinkRefreshScreen();
}

void closeFbink() { fbink_close(fbfd); }

void restoreFbDepth() {
  // TODO: Use execve() family instead of system()
  if (model == "n437" or model == "kt") {
    if (fileExists("/kobo/tmp/inkbox_running") == true) {
      system("/opt/bin/fbink/fbdepth -d 8");
    } else {
      // X11 is running, elsewise there is something wrong ...
      if (model == "kt") {
        system("/opt/bin/fbink/fbdepth -d 32");
      } else {
        system("/opt/bin/fbink/fbdepth -d 16");
      }
    }
  }
}
