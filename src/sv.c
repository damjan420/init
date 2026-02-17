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

service* sv_head = NULL;
int sys_state = SYS_RUNNING;

char* trim(char* str) {
  while(isspace((unsigned char)*str))str++;
  if(*str == 0) return str;
  char* end = str + strlen(str) -1;
  while(end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return str;
}


void execv_line(char* line) {

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


service* sv_parse(const char* sv_path, const char* fname) {

  FILE* sv_fp = fopen(sv_path, "r");
  if(sv_fp == NULL) {
    klog(FAIL, "Unable to open service file %s: %s", sv_path, strerror(errno));
    return NULL;
  };

  service* sv = calloc(1, sizeof(service));

  size_t len;
  char* line = NULL;
  for(int ln=0; getline(&line, &len, sv_fp) > 0; ln++) {

     line[strcspn(line, "\r\n")] = '\0';
     if(strncmp(line, "#", 1) == 0 || strncmp(line, "\0", 1) == 0) continue;

     line = trim(line);

     char* eq = strchr(line, '=');
     if(eq == NULL) {
       klog(INFO, "Line %d in service file %s missing an equal characther '='. skipping line", ln, fname);
       continue;
     };

     *eq = '\0';
     const char* key = trim(line);

     const char* val = trim(eq + 1);

     if(strcmp(key, "exec") == 0) {
       sv->exec = strdup(val);

     }
     else if(strcmp(key, "restart") == 0) {
       printf("%s=%s\n", key, val);
       if(strcmp(val, "always") == 0) sv->restart = SV_RS_ALWAYS;
       else if(strcmp(val, "never") == 0) sv->restart = SV_RS_NEVER;
       else if(strcmp(val, "on-failure") == 0) {
         printf("went here\n");
         sv->restart = SV_RS_ON_FAILURE;
       }
       else {
         klog(INFO ,"unknow restart option %s at line %d in service file %s. Assuming restart=never", val, ln, fname);
         sv->restart = SV_RS_NEVER;
       }

     }
     else {
       klog(INFO, "Unknow key at line %d in service file %s. Skipping line", ln, key);
     }
  }
  fclose(sv_fp);
  if(line) free(line);

  else{
    free(sv);
    return NULL;
  }

  if(sv->exec == NULL) {
    klog(FAIL, "Service file %s missing an exec field", fname);
    return NULL;
  }

  if(sv->restart == 0) {
    klog(FAIL, "Service file %s missing a restart field", fname);
  }

  sv->name = strdup(fname);

  return sv;
}

char* sv_conc_av_path(const char* sv_name) {
  size_t av_path_len = strlen(SV_AVALIVABLE_DIR) + strlen(sv_name) + 6;
  char* sv_av_path = malloc(av_path_len);

  if(sv_av_path == NULL) return NULL;

  snprintf(sv_av_path, av_path_len, "%s/%s.sv", SV_AVALIVABLE_DIR, sv_name);

  return sv_av_path;
}

char* sv_conc_en_path(const char* sv_name) {
  size_t en_path_len = strlen(SV_ENABLED_DIR) + strlen(sv_name) + 6;
  char* sv_en_path = malloc(en_path_len);

  if(sv_en_path == NULL) return NULL;

  snprintf(sv_en_path, en_path_len, "%s/%s.sv", SV_ENABLED_DIR, sv_name);

  return sv_en_path;
}


void sv_parse_enabled(){
  DIR* enabled_dp = opendir(SV_ENABLED_DIR);
  if(enabled_dp == NULL) {
    klog(FAIL, "Unable to open dir %s: %s", SV_ENABLED_DIR, strerror(errno));
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
        klog(FAIL, "Unable to parse file service file %s/%s", SV_ENABLED_DIR, name);
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

void sv_exec(service* sv) {
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
    uint64_t expirations;
    read(sv->stop_timer_fd, &expirations, sizeof(expirations));
    epoll_ctl(epfd, EPOLL_CTL_DEL, sv->restart_timer_fd, NULL);
    close(sv->stop_timer_fd);
    sv->stop_timer_fd = -1;
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

  uint64_t expirations;

  printf("restart: %d, stop: %d\n", sv->restart_timer_fd, sv->stop_timer_fd);

  if(sv->restart_timer_fd > 0) {
    read(sv->restart_timer_fd, &expirations, sizeof(expirations));
    epoll_ctl(epfd, EPOLL_CTL_DEL, sv->restart_timer_fd, NULL);
    close(sv->restart_timer_fd);
    sv->restart_timer_fd = -1;
    sv_exec(sv);
  }
  else if(sv->stop_timer_fd > 0) {;
    read(sv->stop_timer_fd, &expirations, sizeof(expirations));
    epoll_ctl(epfd, EPOLL_CTL_DEL, sv->stop_timer_fd, NULL);
    close(sv->stop_timer_fd);
    sv->stop_timer_fd = -1;
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


int  sv_enable(const char* sv_name, uid_t euid) {


  if(euid != 0) return ERR_PERM;

  char* sv_av_path = sv_conc_av_path(sv_name);

  if(sv_av_path == NULL) return ERR_NO_MEM;

  if(access(sv_av_path, F_OK) != 0) {
    free(sv_av_path);
    return ERR_NOT_AV;
  }

  char* sv_en_path = sv_conc_en_path(sv_name);
  if(sv_en_path == NULL) return ERR_NO_MEM;

  if(access(sv_en_path, F_OK) == 0) {

    free(sv_av_path);
    free(sv_en_path);

    return ERR_ALR_EN;
  }

  symlink(sv_av_path, sv_en_path);

  free(sv_av_path);
  free(sv_en_path);
  return ERR_OK;
}

int sv_disable(const char* sv_name, uid_t euid) {

  if(euid != 0) return ERR_PERM;
  char* sv_en_path = sv_conc_en_path(sv_name);
  if(sv_en_path == NULL) return ERR_NO_MEM;

  if(access(sv_en_path, F_OK) != 0) {
    free(sv_en_path);
    return ERR_NOT_EN;
  }

  remove(sv_en_path);
  free(sv_en_path);
  return ERR_OK;
}

service* sv_get_if_up(const char* sv_name) { // treats SV_RESTART_PENDING as up
  service* current = sv_head;
  while(current) {
    if(strcmp(current->name, sv_name) == 0 &&
    (current->state == SV_UP || current->state == SV_RESTART_PENDING))
      return current;
    current = current->next;
  }
  return NULL;
}

int sv_start(const char* sv_name, uid_t euid) {
  if(euid != 0) return ERR_PERM;

  char* sv_av_path = sv_conc_av_path(sv_name);

  if(sv_av_path == NULL) return ERR_NO_MEM;

  if(access(sv_av_path, F_OK) != 0) {
    free(sv_av_path);
    return ERR_NOT_AV;
  }

  service* sv = sv_parse(sv_av_path, sv_name);

  if(sv == NULL) return ERR_CANT_PARSE;

  if(sv_get_if_up(sv_name) != NULL) return ERR_ALR_UP;

  sv->next = sv_head;
  sv_head = sv;
  sv_exec(sv);

  free(sv_av_path);
  return ERR_OK;
}



int sv_stop(const char* sv_name, uid_t euid) {
  if(euid != 0) return ERR_PERM;
  service* sv = sv_get_if_up(sv_name);
  if(sv != NULL) {

    if(sv->state == SV_RESTART_PENDING) {
      uint64_t expirations;
      read(sv->restart_timer_fd, &expirations, sizeof(expirations));
      epoll_ctl(epfd, EPOLL_CTL_DEL, sv->restart_timer_fd, NULL);
      close(sv->restart_timer_fd);
      sv->restart_timer_fd = -1;
      return ERR_OK;
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

    return ERR_OK;
  }
  return ERR_NOT_UP;
}



service* sv_find_by_name(const char* sv_name) {
  service* current = sv_head;
  while(current) {
    if(strcmp(current->name, sv_name) == 0) return current;
    current = current->next;
  }
  return NULL;
}

int sv_state(const char* sv_name, uid_t euid) {
  if(euid != 0) return ERR_PERM;

  const service* sv = sv_find_by_name(sv_name);
  if(sv == NULL) return STATE_UNKNOWN;
  switch (sv->state) {
    case SV_UP: return STATE_UP;
    case SV_RESTART_PENDING: return STATE_RESTART_PENDING;
    case SV_FAILED: return STATE_FAILED;
    case SV_STOPPED: return STATE_STOPPED;
    case SV_STOPPING: return STATE_STOPPING;
    default: return STATE_UNKNOWN;
  }
}
