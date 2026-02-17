#include <sys/types.h>

#define SV_ENABLED_DIR "/etc/init.d/en"
#define SV_AVALIVABLE_DIR "/etc/init.d/av"


#define MAX_RESTARTS 5
#define STABLE_TRESHOLD 60

enum {
  SV_RS_ALWAYS =1,
  SV_RS_ON_FAILURE,
  SV_RS_NEVER
};

enum {
  SYS_RUNNING = 0,
  SYS_SHUTDOWN,
};
enum {
  SV_UP,
  SV_DOWN,
  SV_FAILED,
  SV_RESTART_PENDING,
  SV_STOPPED,
  SV_STOPPING,
};


typedef struct service {
  char* name;
  char* exec;
  pid_t pid;
  int state;
  int restart;
  int restart_count;
  int restart_timer_fd;
  int stop_timer_fd;
  time_t start;
  struct service* next;
  struct service* prev;
} service;

extern int sys_state;
extern service* sv_head;

void sv_parse_enabled();
void sv_exec_enabled();
void sv_process_status(service* sv, int status);
service* sv_find_by_pid(pid_t pid);
void sv_update_stable();
void sv_handle_restart_or_stop_timer_exp(service* sv);

int  sv_enable(const char* sv_name, uid_t euid);
int sv_disable(const char* sv_name, uid_t euid);
int sv_start(const char* sv_name, uid_t euid);
int sv_stop(const char* sv_name, uid_t euid);
int sv_state(const char* sv_name, uid_t euid);
