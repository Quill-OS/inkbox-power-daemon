#include "wifi.h"
#include "appsFreeze.h"
#include "functions.h"

#include <chrono>
#include <fcntl.h>
#include <linux/module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

const std::string emitter = "wifi";

// https://stackoverflow.com/questions/5947286/how-to-load-linux-kernel-modules-from-c-code
#define deleteModule(name, flags) syscall(__NR_delete_module, name, flags)
#define initModule(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)

extern string model;

void turnOffWifi() {
  string WIFI_MODULE;
  string SDIO_WIFI_PWR_MODULE;
  string WIFI_DEV;

  if (model == "n873" or model == "n236" or model == "n306") {
    WIFI_MODULE = "8189fs";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  } else if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613") {
    WIFI_MODULE = "dhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  } else if (model == "n437") {
    WIFI_MODULE = "bcmdhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "wlan0";
  } else {
    WIFI_MODULE = "dhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  }

  string wifiDevPath = "/sys/class/net/" + WIFI_DEV + "/operstate";
  if (fileExists(wifiDevPath) == true) {
    if (readConfigString("/sys/class/net/" + WIFI_DEV + "/operstate") == "up") {
      writeFileString("/run/was_connected_to_wifi", "true");

      system("killall -9 connect_to_network.sh");
      killProcess("connect_to_network.sh");
      killProcess("dhcpcd");
      killProcess("wpa_supplicant");
      killProcess("udhcpc");

      if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613" or model == "n437") {
        system("/bin/wlarm_le down");
      } else {
        string turnOffInterface = "/sbin/ifconfig " + WIFI_DEV + " down";
        system(turnOffInterface.c_str());
      }
    } else {
      log("Wi-Fi is already off, but modules are still live", emitter);
    }
    if (deleteModule(WIFI_MODULE.c_str(), O_NONBLOCK) != 0) {
      log("Can't unload module: " + WIFI_MODULE, emitter);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (deleteModule(SDIO_WIFI_PWR_MODULE.c_str(), O_NONBLOCK) != 0) {
      log("Can't unload module: " + SDIO_WIFI_PWR_MODULE, emitter);
    }
  } else {
    log("Is Wi-Fi already off?", emitter);
  }
}

void turnOnWifi() {
  string WIFI_MODULE;
  string SDIO_WIFI_PWR_MODULE;
  string WIFI_DEV;

  if (model == "n873" or model == "n236" or model == "n306") {
    WIFI_MODULE = "/modules/wifi/8189fs.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/drivers/mmc/card/sdio_wifi_pwr.ko";
    WIFI_DEV = "eth0";
  } else if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613") {
    WIFI_MODULE = "/modules/dhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/sdio_wifi_pwr.ko";
    WIFI_DEV = "eth0";
  } else if (model == "n437") {
    WIFI_MODULE = "/modules/wifi/bcmdhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/drivers/mmc/card/sdio_wifi_pwr.ko";
    WIFI_DEV = "wlan0";
  } else {
    WIFI_MODULE = "/modules/dhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/sdio_wifi_pwr.ko";
    WIFI_DEV = "eth0";
  }
  if (fileExists("/run/was_connected_to_wifi") == true) {
    if (readConfigString("/run/was_connected_to_wifi") == "true") {
      loadModule(WIFI_MODULE);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      loadModule(SDIO_WIFI_PWR_MODULE);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      string wifiInterfaceUp = "/sbin/ifconfig " + WIFI_DEV + " up";
      system(wifiInterfaceUp.c_str());

      if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613" or model == "n437") {
        system("/bin/wlarm_le up");
      }
    }

    if (fileExists("/data/config/17-wifi_connection_information/essid") ==
            true and
        fileExists("/data/config/17-wifi_connection_information/passphrase") ==
            true) {
      string essid = readConfigString("/data/config/17-wifi_connection_information/essid");
      string passphrase = readConfigString("/data/config/17-wifi_connection_information/passphrase");
      string reconnection = "/usr/local/bin/wifi/connect_to_network.sh " + essid + " " + passphrase + " &";
      system(reconnection.c_str());
    }
    remove("/run/was_connected_to_wifi");
  }
}

void loadModule(string path) {
  size_t image_size;
  struct stat st;
  void * image;
  const char * params = "";
  int fd = open(path.c_str(), O_RDONLY);
  puts("init");
  fstat(fd, &st);
  image_size = st.st_size;
  image = malloc(image_size);
  read(fd, image, image_size);
  close(fd);
  if (initModule(image, image_size, params) != 0) {
    log("Couldn't init module " + path, emitter);
    exit(EXIT_FAILURE);
  }
  free(image);
}