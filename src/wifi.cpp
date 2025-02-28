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
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

const std::string emitter = "wifi";

extern string model;
extern pid_t connectToWifiPid;
extern bool reconnectWifi;

void turnOffWifi() {
  string WIFI_MODULE;
  string SDIO_WIFI_PWR_MODULE;
  string WIFI_DEV;

  if (model == "n873" or model == "n236" or model == "n306") {
    WIFI_MODULE = "8189fs";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  }
  else if (model == "n418") {
    WIFI_MODULE = "8723ds";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "wlan0";
  }
  else if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613") {
    WIFI_MODULE = "dhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  } 
  else if (model == "n437") {
    WIFI_MODULE = "bcmdhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "wlan0";
  } 
  else {
    WIFI_MODULE = "dhd";
    SDIO_WIFI_PWR_MODULE = "sdio_wifi_pwr";
    WIFI_DEV = "eth0";
  }

  // Managing zombies is a bit... problematic
  log("connectToWifiPid is " + to_string(connectToWifiPid), emitter);
  if (connectToWifiPid != 0) {
    killProcess("connect_to_network.sh");
    log("Collecting connect_to_network.sh zombie", emitter);
    waitpid(connectToWifiPid, 0, 0);
    log("Collected connect_to_network.sh zombie", emitter);
    connectToWifiPid = 0;
  }
  else {
    log("No need to collect zombie process: connectToWifi", emitter);
  }

  string wifiDevPath = "/sys/class/net/" + WIFI_DEV + "/operstate";
  if (fileExists(wifiDevPath) == true) {
    string wifiState = readConfigString("/sys/class/net/" + WIFI_DEV + "/operstate");
    // 'dormant' is when the Wi-Fi interface is disconnected from a network, but is still on
    if (wifiState == "up" or wifiState == "dormant") {
      if (reconnectWifi == true) {
        // To be 100% sure
        remove("/run/was_connected_to_wifi");
        writeFileString("/run/was_connected_to_wifi", "true");
      }

      killProcess("toggle.sh");
      killProcess("turn_on_with_stats.sh");
      killProcess("connection_manager.sh");
      killProcess("get_dhcp.sh");
      killProcess("prepare_changing_wifi.sh");
      killProcess("prepare_network.sh");
      killProcess("wifi_information.sh");
      killProcess("connect_to_network.sh");
      
      killProcess("dhcpcd");
      killProcess("wpa_supplicant");
      killProcess("udhcpc");

      if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613" or model == "n437") {
        string wlarm_lePath = "/bin/wlarm_le";
        const char *args[] = {wlarm_lePath.c_str(), "down", nullptr};
        int fakePid = 0;
        posixSpawnWrapper(wlarm_lePath.c_str(), args, true, &fakePid);
      } 
      else {
        string ifconfigPath = "/sbin/ifconfig";
        const char *args[] = {ifconfigPath.c_str(), WIFI_DEV.c_str(), "down", nullptr};
        int fakePid = 0;
        posixSpawnWrapper(ifconfigPath.c_str(), args, true, &fakePid);
      }
    } 
    else {
      log("Wi-Fi is already off, but modules are still live", emitter);
    }
    if (deleteModule(WIFI_MODULE.c_str(), O_NONBLOCK) != 0) {
      log("Can't unload module: " + WIFI_MODULE, emitter);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (deleteModule(SDIO_WIFI_PWR_MODULE.c_str(), O_NONBLOCK) != 0) {
      log("Can't unload module: " + SDIO_WIFI_PWR_MODULE, emitter);
    }
  } 
  else {
    log("Wi-Fi is turned off", emitter);
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
  }
  else if (model == "n418") {
    WIFI_MODULE = "/modules/4.1.15-inkbox/kernel/8723ds.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/4.1.15-inkbox/kernel/drivers/mmc/card/sdio_wifi_pwr.ko";
    WIFI_DEV = "wlan0";
  }
  else if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613") {
    WIFI_MODULE = "/modules/dhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/sdio_wifi_pwr.ko";
    WIFI_DEV = "eth0";
  } 
  else if (model == "n437") {
    WIFI_MODULE = "/modules/wifi/bcmdhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/drivers/mmc/card/sdio_wifi_pwr.ko";
    WIFI_DEV = "wlan0";
  } 
  else {
    WIFI_MODULE = "/modules/dhd.ko";
    SDIO_WIFI_PWR_MODULE = "/modules/sdio_wifi_pwr.ko";
    WIFI_DEV = "eth0";
  }
  if (fileExists("/run/was_connected_to_wifi") == true) {
    if (readConfigBool("/run/was_connected_to_wifi") == true) {
      loadModule(WIFI_MODULE);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      loadModule(SDIO_WIFI_PWR_MODULE);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      
      if (model == "n705" or model == "n905b" or model == "n905c" or model == "n613" or model == "n437") {
        string wlarm_lePath = "/bin/wlarm_le";
        const char *args[] = {wlarm_lePath.c_str(), "up", nullptr};
        int fakePid = 0;
        posixSpawnWrapper(wlarm_lePath.c_str(), args, true, &fakePid);
      }
      else {
        string ifconfigPath = "/sbin/ifconfig";
        const char *args[] = {ifconfigPath.c_str(), WIFI_DEV.c_str(), "up", nullptr};
        int fakePid = 0;
        posixSpawnWrapper(ifconfigPath.c_str(), args, true, &fakePid);
      }
    }

    if (fileExists("/data/config/17-wifi_connection_information/essid") == true and
        fileExists("/data/config/17-wifi_connection_information/passphrase") == true) {

      string essid = readConfigString("/data/config/17-wifi_connection_information/essid");
      string passphrase = readConfigString("/data/config/17-wifi_connection_information/passphrase");

      // If this is needed anywhere else, a function needs to be created
      string reconnectionScriptPath = "/usr/local/bin/wifi/connect_to_network.sh";
      const char *args[] = {reconnectionScriptPath.c_str(), essid.c_str(), passphrase.c_str(), nullptr};
      posixSpawnWrapper(reconnectionScriptPath.c_str(), args, false, &connectToWifiPid);
    }
  }
}

bool loadModule(string path) {
  size_t image_size;
  struct stat st;
  void *image;
  const char *params = "";
  int fd = open(path.c_str(), O_RDONLY);
  fstat(fd, &st);
  image_size = st.st_size;
  image = malloc(image_size);
  read(fd, image, image_size);
  close(fd);
  if (initModule(image, image_size, params) != 0) {
    log("Couldn't init module " + path, emitter);
    return false;
  }
  free(image);
  return true;
}
