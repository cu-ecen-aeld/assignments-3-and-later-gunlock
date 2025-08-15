#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char **argv) {
  openlog(NULL, 0, LOG_USER);  // optional given using defaults

  if (argc < 3) {
    syslog(LOG_ERR, "Invalid argument number: %d", (argc - 1));
    return 1;
  }

  const char *filepath = argv[1];
  const char *writestr = argv[2];

  int fd = open(filepath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fd == -1) {
    syslog(LOG_ERR, "Failed to open %s: %s", filepath, strerror(errno));
    return -1;
  }

  ssize_t nr = write(fd, writestr, strlen(writestr));
  if (nr == -1) {
    syslog(LOG_ERR, "Failed writing \"%s\" to %s: %s", writestr, filepath, strerror(errno));
    return -1;
  }

  syslog(LOG_DEBUG, "Writing \"%s\" to %s", writestr, filepath);

  if (close(fd) == -1) {
    syslog(LOG_ERR, "Failure on close(): %s", strerror(errno));
    return -1;
  }

  return 0;
}
