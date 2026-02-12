#include <sys/types.h>

#define MAX_RESTARTS 5

enum {
  ALWAYS,
  ON_FAILURE,
  NEVER
};

enum {
  RUNNING,
  SHUTDOWN,
};
enum {
  UP,
  FAILED,
  RESTART_PENDING,
};


typedef struct service {
  char* name;
  char* exec;
  pid_t pid;
  int state;
  int restart;
  int restart_count;
  time_t started;
  time_t next_restart;
  struct service* next;
  struct service* prev;
} service;

extern int sys_state;
extern service* sv_head;

void sv_parse_enabled();
void sv_exec_enabled();
void sv_restart();
void sv_schedule_restart(service* sv, int status);
struct timespec* sv_get_next_restart();
service* sv_find_by_pid(pid_t pid);
