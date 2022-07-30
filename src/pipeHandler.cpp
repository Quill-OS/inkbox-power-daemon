#include "pipeHandler.h"
#include "functions.h"

#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <experimental/filesystem>
#include <fcntl.h>
#include <sys/mount.h>

const std::string emitter = "pipeHandler";

string pipeDirectoryPath = "/run/ipd/";
string pipePath = pipeDirectoryPath + "fifo";

void startPipeServer() {
  log("Starting named pipe server", emitter);

  if (dirExists("/run/ipd") == false) {
    experimental::filesystem::create_directory(pipeDirectoryPath);
    log("Creating /run/ipd", emitter);
  }

  // Size is important here
  string options = "size=8K,uid=0,gid=0,mode=744";

  // Creating tmpfs
  // Note: Creating this tmpfs will kill the bind mount to /kobo/dev/ipd. The bind mount needs to be done after it
  int rc = 0;
  rc = mount("tmpfs", pipeDirectoryPath.c_str(), "tmpfs", 0, options.c_str());
  if (rc != 0) {
    log("tmpfs creation failed", emitter);
  }

  // Creating the named pipe
  mkfifo((const char *)pipePath.c_str(), 0666);
}

void sleepPipeSend() {
  log("Sending message", emitter);
  int fd = open(pipePath.c_str(), O_RDWR); // O_WRONLY // https://stackoverflow.com/questions/24099693/c-linux-named-pipe-hanging-on-open-with-o-wronly

  string testString = "start";
  write(fd, testString.c_str(), 5);
  close(fd);
}

void restorePipeSend() {
  log("Sending message", emitter);
  int fd = open(pipePath.c_str(), O_RDWR); // O_WRONLY // https://stackoverflow.com/questions/24099693/c-linux-named-pipe-hanging-on-open-with-o-wronly

  string testString = "stop0";
  write(fd, testString.c_str(), 5);
  close(fd);
}
