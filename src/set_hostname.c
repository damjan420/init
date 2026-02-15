#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "klog.h"

char* read_hostname() {
  FILE* fp = fopen("/etc/hostname", "r");
  if(fp == NULL) {
    return NULL;
  }
  size_t len;
  char* line = NULL;
  if(getline(&line, &len, fp) == -1) {
    free(line);
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  line[strcspn(line, "\r\n")] = '\0';
  return line;
}

int main() {
  char* hostname = read_hostname();

  if(hostname == NULL) {
    klog(FAIL, "failed to read etc/hostname: %s", strerror(errno));
    return -1;
  } else {
    size_t len = strlen(hostname);
    if(sethostname(hostname, len) != 0) {
      klog(FAIL, "failed to set hostname: %s", strerror(errno));
      return -1;
    } else {
      klog(OK, "set hostname to %s", hostname);
      return 0;
    }
  }

}
