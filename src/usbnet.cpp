#include "usbnet.h"
#include "wifi.h" // For loading modules
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
const std::string emitter = "usbnet";

extern bool wasUsbNetOn;
extern string model;

void disableUsbNet() {
  if (wasUsbNetOn == true) {
    log("Disabling USB networking", emitter);
    int timestamp = 200;
    if (model == "kt") {
      executeCommand("modprobe -r g_ether");
    } else {
      // i.MX 507/508 : Arc USBOTG Device Controller Driver
      if(deleteModule(string("arcotg_udc").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: arcotg_udc", emitter);
      }
      // USB Ethernet gadget
      if (deleteModule(string("g_ether").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: g_ether", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      // Helpers for the above module
      if (deleteModule(string("usb_f_rndis").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_rndis", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_ecm_subset").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_ecm_subset", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_eem").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_eem", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("usb_f_ecm").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: usb_f_ecm", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("u_ether").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: u_ether", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("libcomposite").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: libcomposite", emitter);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(timestamp));
      //
      if (deleteModule(string("configfs").c_str(), O_NONBLOCK) != 0) {
        log("Can't unload module: configfs", emitter);
      }
    }
    std::remove("/run/openrc/started/usbnet");
  }
}

void startUsbNet() {
    if(wasUsbNetOn == true) {
      log("Restoring USB networking", emitter);
      executeCommand("service usbnet start");
    }
}
