#include "pipeHandler.h"
#include "functions.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <experimental/filesystem>
#include <fcntl.h>
#include <sys/mount.h>

const std::string emitter = "pipeHandler";

void startPipeServer() {
  log("Starting named pipe server", emitter);

  if (dirExists("/run/ipd") == false) {
    log("Creating /run/ipd", emitter);
    experimental::filesystem::create_directory("/run/ipd");
    experimental::filesystem::create_directory("/kobo/run/ipd");
    // Creating the named pipe (First-In-First-Out)
    const char * pipe = "/run/ipd/fifo";
    mkfifo(pipe, 0666);
  }
}

void sleepPipeSend() {
  log("Sending message", emitter);
  const char * pipe = "/run/ipd/fifo";
  int fd = open(pipe, O_RDWR); // O_WRONLY // https://stackoverflow.com/questions/24099693/c-linux-named-pipe-hanging-on-open-with-o-wronly

  string testString = "start";
  write(fd, testString.c_str(), 5);
  close(fd);
}

void restorePipeSend() {
  log("Sending message", emitter);
  const char * pipe = "/run/ipd/fifo";
  int fd = open(pipe, O_RDWR); // O_WRONLY // https://stackoverflow.com/questions/24099693/c-linux-named-pipe-hanging-on-open-with-o-wronly

  string testString = "stop0";
  write(fd, testString.c_str(), 5);
  close(fd);
}
