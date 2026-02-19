#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include "klog.h"
#include "sv.h"
#include "init.h"
#include "cgroup.h"
#include "ctl.h"

#define MY_MALLOC_IMPLEMENTATION
#include "my_malloc.h"

service* sv_head = NULL;
int sys_state = SYS_RUNNING;

static char* trim(char* str) {
  while(isspace((unsigned char)*str))str++;
  if(*str == 0) return str;
  char* end = str + strlen(str) -1;
  while(end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return str;
}

static void sv_parse_file_line(service* sv, char* line, int ln, const char* fname) {
  if(line == NULL) return;

  line[strcspn(line, "\r\n")] = '\0';
  if(strncmp(line, "#", 1) == 0 || strncmp(line, "\0", 1) == 0) return;

  line = trim(line);

  char* eq = strchr(line, '=');
  if(eq == NULL) {
    klog(INFO, "line %d in service file %s.sv missing an equal characther '='. skipping line", ln, fname);
    return;
  };

  *eq = '\0';
  const char* key = trim(line);

  const char* val = trim(eq + 1);

  if(strcmp(key, "exec") == 0) {
    sv->exec = strdup(val);
  }
  else if(strcmp(key, "restart") == 0) {
    if(strcmp(val, "always") == 0) sv->restart = SV_RS_ALWAYS;
    else if(strcmp(val, "never") == 0) sv->restart = SV_RS_NEVER;
    else if(strcmp(val, "on-failure") == 0) {
      sv->restart = SV_RS_ON_FAILURE;
    }
    else {
      klog(INFO ,"unknow restart option %s at line %d in service file %s. Assuming restart=never", val, ln, fname);
      sv->restart = SV_RS_NEVER;
    }
  }
  else if(strcmp(key, "pids_max") == 0) {
    if(strcmp(val, "max") == 0) sv->pids_max = -1;
    else {
      char* end;
      int64_t i  = strtol(val, &end, 10);
      if(val == end || *end != '\0') {
        klog(INFO, "failed to parse value for pids_max in service file %s.sv. Assuming pids_max=max", fname);
      }
      else if(errno == ERANGE) {
        klog(INFO, "value for pids_max in service file %s.sv out of range. Assuming pids_max=max", fname);
      }
      else {
        if(i < 0) {
          klog(INFO, "value for pids_max in service file %s.sv negative. Assuming pids_max=max", fname);
        }
        else sv->pids_max = i;
      }
    }
  }
  else if(strcmp(key, "memory_max") == 0) {
    if(strcmp(val, "max") == 0) sv->memory_max = -1;
    else {
      char* end;
      int64_t i  = strtol(val, &end, 10);
      if(val == end || *end != '\0') {
        klog(INFO, "failed to parse value for memory_max in %s.sv. Assuming pids_max=max", fname);
      }
      else if(errno == ERANGE) {
        klog(INFO, "value for memroy_max in %s.sv out of range. Assuming pids_max=max", fname);
      }
      else {
        if(i < 0) {
          klog(INFO, "value for memory_max in %s.sv negative. Assuming pids.max=max", fname);
        } else sv->memory_max = i;
      }
    }
  }
  else if(strcmp(key, "cpu_quota") == 0) {
    if(strcmp(val, "max") == 0) sv->cpu_quota = -1;
    else {
      char* end;
      int64_t i  = strtol(val, &end, 10);
      if(val == end || *end != '\0') {
        klog(INFO, "failed to parse value for cpu_quota in %s.sv. Assuming cpu_quota=max", fname);
      }
      else if(errno == ERANGE) {
        klog(INFO, "value for cpu_quota in %s.sv out of range. Assuming cpu_quota=max", fname);
      }
      else {
        if(i < 0) {
          klog(INFO, "value for cpu_quota in %s.sv negative. Assuming cpu_quota=max", fname);
        } else sv->cpu_quota = i;
      }
    }
  }
  else if(strcmp(key, "cpu_period") == 0) { ;
    char* end;
    int64_t i  = strtol(val, &end, 10);
    if(val == end || *end != '\0') {
      klog(INFO, "failed to parse value for cpu_period in %s.sv. Assuming cpu_perdiod=100000", fname);
    }
    else if(errno == ERANGE) {
      klog(INFO, "value for cpu_period in %s.sv out of range. Assuming period=100000", fname);
    }
    else {
      if(i < 0) {
        klog(INFO, "value for cpu_period in %s.sv negative. Assuming cpu_period=max", fname);
      } else sv->cpu_period = i;
    }
  }
  else {
    klog(INFO, "unknow key at line %d in service file %s.sv. Skipping line", ln, key);
  }
}

static service* sv_init() {
  service* sv = my_calloc(1, sizeof(service));
  if(sv == NULL) return NULL;

  sv->restart_count = 0;
  sv->restart_timer_fd = -1;
  sv->stop_timer_fd = -1;
  sv->pids_max = -1;
  sv->memory_max = -1;
  sv->cpu_quota = -1;
  sv->cpu_period = 100000;
  return sv;
};

static service* sv_parse(const char* sv_path, const char* fname) {
  if(fname == NULL) return NULL;
  FILE* sv_fp = fopen(sv_path, "r");
  if(sv_fp == NULL) {
    klog(FAIL, "failed to open service file %s.sv: %s", sv_path, strerror(errno));
    return NULL;
  };
  service* sv = sv_init();
  if(sv == NULL) {
    fclose(sv_fp);
    return NULL;
  }


  size_t len;
  char* line = NULL;
  for(int ln=0; getline(&line, &len, sv_fp) > 0; ln++) {
     sv_parse_file_line(sv, line, ln, fname);
  }
  fclose(sv_fp);
  if(line) free(line);

  else{
    my_free(sv);
    return NULL;
  }

  if(sv->exec == NULL) {
    klog(FAIL, "service file %s.sv missing an exec field", fname);
    return NULL;
  }

  if(sv->restart == 0) {
    klog(FAIL, "service file %s.sv missing a restart field", fname);
    return NULL;
  }

  sv->name = strdup(fname);

  return sv;
}

static char* sv_conc_av_path(const char* sv_name) {
  size_t av_path_len = strlen(SV_AVALIVABLE_DIR) + strlen(sv_name) + 6;
  char* sv_av_path = malloc(av_path_len);

  if(sv_av_path == NULL) return NULL;

  snprintf(sv_av_path, av_path_len, "%s/%s.sv", SV_AVALIVABLE_DIR, sv_name);

  return sv_av_path;
}

static char* sv_conc_en_path(const char* sv_name) {
  size_t en_path_len = strlen(SV_ENABLED_DIR) + strlen(sv_name) + 6;
  char* sv_en_path = malloc(en_path_len);

  if(sv_en_path == NULL) return NULL;

  snprintf(sv_en_path, en_path_len, "%s/%s.sv", SV_ENABLED_DIR, sv_name);

  return sv_en_path;
}

static void execv_line(char* line) {

  char *args[64];
  int i = 0;

  char *token = strtok(line, " ");
  while (token != NULL && i < 63) {
    args[i++] = token;
    token = strtok(NULL, " ");
  }
  if(i != 0) args[i]= NULL;
  else {
    args[0] = line;
    args[1] = NULL;
  }

  execv(args[0], args);
}

static void sv_exec(service* sv) {
  if(sv == NULL) return;
  sv->pid = fork();
  if(sv->pid == 0) {
    klog(INFO, "starting service %s", sv->name);
    cg_start(sv);
    execv_line(sv->exec);
    klog(FAIL, "failed to start service %s: %s", sv->name, strerror(errno));
    _exit(-1);
  }
  sv->state = SV_UP;
  sv->start = time(NULL);
}


static service* sv_find_by_name(const char* sv_name) {
  service* current = sv_head;
  while(current) {
    if(strcmp(current->name, sv_name) == 0) return current;
    current = current->next;
  }
  return NULL;
}

static service* sv_get_if_up(const char* sv_name) { // treats SV_RESTART_PENDING as up
  service* current = sv_head;
  while(current) {
    if(strcmp(current->name, sv_name) == 0 &&
    (current->state == SV_UP || current->state == SV_RESTART_PENDING))
      return current;
    current = current->next;
  }
  return NULL;
}

static void sv_clear_timer(int* timer_fd) {
  uint64_t expirations;
  read(*timer_fd, &expirations, sizeof(uint64_t));
  epoll_ctl(epfd, EPOLL_CTL_DEL, *timer_fd, NULL);
  close(*timer_fd);
  *timer_fd = -1;
}

void sv_parse_enabled(){
  DIR* enabled_dp = opendir(SV_ENABLED_DIR);
  if(enabled_dp == NULL) {
    klog(FAIL, "failed to open dir %s: %s", SV_ENABLED_DIR, strerror(errno));
    return;

  }
  
  const struct dirent* entry;

  while((entry = readdir(enabled_dp)) != NULL) {
    char* name = (char*)entry->d_name;

    size_t nlen = strlen(entry->d_name);

    if(strncmp(&(name[nlen -2]), "s", 1) == 0 && strncmp(&(name[nlen -1]), "v", 1) == 0 && (entry->d_type == DT_REG || entry->d_type == DT_LNK)) {

      name[nlen - 3] = '\0';

      char * sv_en_path  = sv_conc_en_path(name);
      service* sv = sv_parse(sv_en_path, name);
      free(sv_en_path);
      if(sv == NULL) {
        klog(FAIL, "failed to parse file service file %s.sv", name);
      }


      if(sv_head == NULL) {
        sv_head = sv;
      }
      else {
        sv->next = sv_head;
        sv_head = sv;
      }
    }
  };
}

void sv_exec_enabled() {
  service* current = sv_head;

  while(current) {
    printf("%s\n", current->name);
    sv_exec(current);
    current = current->next;
  }
}

service* sv_find_by_pid(pid_t pid) {
  service* current = sv_head;
  while(current) {
    if(current->pid == pid) return current;
    current = current->next;
  }
  return NULL;
}


void sv_process_status(service* sv, int status) {

  if(sys_state == SYS_SHUTDOWN) return;
  if(sv == NULL) return;

  bool failed =  (
                  (WIFEXITED(status) && WEXITSTATUS(status) != 0 )
                  || (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM)
                  );
  if(sv->state == SV_FAILED) return;
  if(sv->state == SV_STOPPED) return;

  else if(sv->state == SV_STOPPING) {
    sv_clear_timer(&(sv->stop_timer_fd));
    sv->state = SV_STOPPED;
    return;
  }

  if(sv->restart == SV_RS_NEVER) {
    if(failed) sv->state = SV_FAILED;
    else       sv->state = SV_DOWN;
  }

  else if((sv->restart == SV_RS_ALWAYS) || ( sv->restart == SV_RS_ON_FAILURE && failed)) {
    if(sv->restart_count >= MAX_RESTARTS) {
      klog(FAIL, "service %s chrased 5 times in 25 seconds. marking as FAILED", sv->name);
      sv->state=SV_FAILED;
      return;
    }
    klog(FAIL, "failed: %d", failed);
    klog(FAIL, "service %s crashed. Next restart in 5 seconds", sv->name);

      sv->restart_count++;

      sv->state = SV_RESTART_PENDING;
      sv->restart_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
      struct itimerspec its = {0};
      its.it_value.tv_sec = 5;
      its.it_value.tv_nsec = 0;
      timerfd_settime(sv->restart_timer_fd, 0, &its,NULL);

      struct epoll_event service_event = {.data.ptr = sv, .events=EPOLLIN};

      epoll_ctl(epfd, EPOLL_CTL_ADD, sv->restart_timer_fd, &service_event);

  }
}


void sv_handle_restart_or_stop_timer_exp(service* sv) {

  if(sv->restart_timer_fd > 0) {
    sv_clear_timer(&(sv->restart_timer_fd));
    sv_exec(sv);
  }
  else if(sv->stop_timer_fd > 0) {
    sv_clear_timer(&(sv->stop_timer_fd));
    sv->state = SV_STOPPED;
    cg_kill(sv);
  }

}


void sv_update_stable() { // if restart_count = 0 service is stable
  service* current = sv_head;
  time_t now = time(NULL);

  while(current) {
    if(now - current->start >= 60) {
      current->restart_count = 0;
    }
    current = current->next;
  }
}


void sv_enable(const char* sv_name, uid_t euid, res_payload* payload_buf) {


  if(euid != 0) {
    payload_buf->err = ERR_PERM;
    return;
  }
  char* sv_av_path = sv_conc_av_path(sv_name);

  if(sv_av_path == NULL){
      payload_buf->err = ERR_NO_MEM;
      return;
    };

  if(access(sv_av_path, F_OK) != 0) {
    free(sv_av_path);
    payload_buf->err = ERR_NOT_AV;
    return;
  }

  char* sv_en_path = sv_conc_en_path(sv_name);
  if(sv_en_path == NULL) {
      payload_buf->err = ERR_NO_MEM;
      return;
    }

  if(access(sv_en_path, F_OK) == 0) {

    free(sv_av_path);
    free(sv_en_path);
    payload_buf->err = ERR_ALR_EN;
    return;
  }

  symlink(sv_av_path, sv_en_path);

  free(sv_av_path);
  free(sv_en_path);
  payload_buf->err = ERR_OK;
}

void sv_disable(const char* sv_name, uid_t euid, res_payload* payload_buf) {
  if(euid != 0) {
    payload_buf->err = ERR_PERM;
    return;
  };
  char* sv_en_path = sv_conc_en_path(sv_name);
  if(sv_en_path == NULL) {
    payload_buf->err = ERR_NO_MEM;
    return;
  };

  if(access(sv_en_path, F_OK) != 0) {
    free(sv_en_path);
    payload_buf->err= ERR_NOT_EN;
    return;
  }

  remove(sv_en_path);
  free(sv_en_path);
  payload_buf->err = ERR_OK;
}


void sv_start(const char* sv_name, uid_t euid, res_payload* payload_buf) {

  if(payload_buf == NULL) return;

  if(euid != 0) {
    payload_buf->err = ERR_PERM;
    return;
  };

  char* sv_av_path = sv_conc_av_path(sv_name);

  if(sv_av_path == NULL)  {
    payload_buf->err = ERR_NO_MEM;
    return;
  };

  if(access(sv_av_path, F_OK) != 0) {
    free(sv_av_path);
    payload_buf->err = ERR_NOT_AV;
    return;
  }

  service* sv = sv_parse(sv_av_path, sv_name);

  if(sv == NULL) {
    payload_buf->err = ERR_CANT_PARSE;
    return;
  };

  if(sv_get_if_up(sv_name) != NULL) {
    payload_buf->err = ERR_ALR_UP;
  };

  sv->next = sv_head;
  sv_head = sv;
  sv_exec(sv);

  free(sv_av_path);
  payload_buf->err = ERR_OK;
  payload_buf->state = sv->state;
  payload_buf->pid = sv->pid;
}



void sv_stop(const char* sv_name, uid_t euid, res_payload* payload_buf) {

  if(payload_buf == NULL) return;

  if(euid != 0) {
    payload_buf->err = ERR_PERM;
    return;
  };

  service* sv = sv_get_if_up(sv_name);
  if(sv != NULL) {

    if(sv->state == SV_RESTART_PENDING) {
      sv_clear_timer(&(sv->restart_timer_fd));
      sv->state = SV_STOPPED;
      payload_buf->err = ERR_OK;
      payload_buf->state = SV_STOPPED;
      payload_buf->pid = sv->pid;
    }

    kill(sv->pid, SIGTERM);
    sv->state = SV_STOPPING;

    sv->stop_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its = {0};
    its.it_value.tv_sec = 5;
    its.it_value.tv_nsec = 0;
    timerfd_settime(sv->stop_timer_fd, 0, &its,NULL);

    struct epoll_event service_event = {.data.ptr = sv, .events=EPOLLIN};
    epoll_ctl(epfd, EPOLL_CTL_ADD, sv->stop_timer_fd, &service_event);

    payload_buf->err = ERR_OK;
    payload_buf->state = SV_STOPPED;
    payload_buf->pid = sv->pid;
    return;
  }
  payload_buf->err = ERR_NOT_UP;
}


void sv_state(const char* sv_name, uid_t euid, res_payload* payload_buf) {

  if(payload_buf == NULL) return;

  if(euid != 0) {
    payload_buf->err = ERR_PERM;
  };

  const service* sv = sv_find_by_name(sv_name);

  if(sv == NULL) {
    payload_buf->state = STATE_UNKNOWN;
    return;
  }

  switch (sv->state) {
  case SV_UP:              payload_buf->state = STATE_UP; break;
  case SV_DOWN:            payload_buf->state = STATE_DOWN; break;
  case SV_FAILED:          payload_buf->state = STATE_FAILED; break;
  case SV_RESTART_PENDING: payload_buf->state = STATE_RESTART_PENDING; break;
  case SV_STOPPING:        payload_buf->state = STATE_STOPPING; break;
  case SV_STOPPED:         payload_buf->state = STATE_STOPPED; break;
  default:                 payload_buf->state = STATE_UNKNOWN; break;
  }
}
