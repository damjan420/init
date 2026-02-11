#include <sys/types.h>

enum {
  ALWAYS,
  ON_FAILURE,
  NEVER
};

enum {
  RUNNING,
  SHUTDOWN,
};

typedef struct service {
  char* name;
  char* exec;
  pid_t pid;
  int restart;
  int restart_count;
  struct service* next;
  struct service* prev;
} service;

extern int sys_state;
extern service* sv_head;
