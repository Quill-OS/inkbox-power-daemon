#include "usbnet.h"
#include "Wifi.h" // for loading modules
#include "functions.h"

#include <fcntl.h>
#include <iostream>
#include <linux/module.h>
#include <stdexcept>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define deleteModule(name, flags) syscall(__NR_delete_module, name, flags)

extern bool wasUsbNetOn;

extern string model;

void disableUsbNet() {
  if (wasUsbNetOn == true) {
    log("Disabling usbnet");
    int timestamp = 200;
    if (model == "kt") {
      executeCommand("modprobe -r g_ether");
    } else {
      //
      if (deleteModule(string("g_ether").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: g_ether");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_rndis").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_rndis");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_ecm_subset").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_ecm_subset");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_eem").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_eem");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_ecm").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_ecm");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("u_ether").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: u_ether");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("libcomposite").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: libcomposite");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("configfs").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: configfs");
      }
    }
    std::remove("/run/openrc/started/usbnet");
  }
}

void startUsbNet() {
    if(wasUsbNetOn == true) {
      log("Restoring USB networking");
      executeCommand("service usbnet start");
    }
}
