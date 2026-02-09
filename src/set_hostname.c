#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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
    fprintf(stderr, "[ FAIL ] Read etc/hostname: %s\n", strerror(errno));
    return -1;
  } else {
    size_t len = strlen(hostname);
    if(sethostname(hostname, len) != 0) {
      fprintf(stderr, "[ FAIL ] Set hostname: %s\n", strerror(errno));
      return -1;
    } else {
      fprintf(stderr, "[ OK ] Set hostname to %s\n", hostname);
      return 0;
    }
  }

}
