#include <sys/types.h>

typedef struct service {
  char* name;
  char* exec;
  pid_t pid;
  struct service* next;
  struct service* prev;
} service;
