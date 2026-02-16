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

#include "klog.h"
#include "sv.h"
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
       if(strcmp(val, "always") == 0) sv->restart = SV_RS_ALWAYS;
       else if(strcmp(val, "never") == 0) sv->restart = SV_RS_NEVER;
       else if(strcmp(val, "on-failure") == 0) sv->restart = SV_RS_ON_FAILURE;
       else {
         klog(INFO ,"Unknow restart option %s at line %d in service file %s. Assuming restart=never", val, ln, fname);
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
  pid_t pid = fork();
  if(pid == 0) {
    klog(INFO, "starting service %s", sv->name);
    execv_line(sv->exec);
    klog(FAIL, "failed to start service %s: %s", sv->name, strerror(errno));
    _exit(-1);
  }
  sv->pid = pid;
  sv->state = SV_UP;
  sv->start = time(NULL);
}

void sv_exec_enabled() {
  service* current = sv_head;
  while(current) {
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


void sv_restart() {
  time_t now = time(NULL);
  service* current = sv_head;

  while(current) {
    if(current->state == SV_RESTART_PENDING && current->next_restart >= now){
      current->restart_count++;
      sv_exec(current);

    }
    current = current->next;
  }
}

void sv_schedule_restart(service* sv, int status) {
   if(sys_state == SYS_SHUTDOWN) return;
   if(sv == NULL) return;
   if(sv->restart == SV_RS_NEVER) return;

  if(sv->restart_count >= MAX_RESTARTS) {
    klog(FAIL, "service %s crashed 5 times in 25 seconds. Marking as FAILED", sv->name);
    sv->state = SV_FAILED;
    sv->next_restart = 0;
    return;
  }

  else if ( (sv->restart == SV_RS_ALWAYS) ||
          ( (sv->restart == SV_RS_ON_FAILURE) && ( (WIFEXITED(status) && WEXITSTATUS(status) != 0 )
          || (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) ) ) ){

      klog(FAIL, "service %s crashed. Next restart in 5 seconds", sv->name);
      time_t now = time(NULL);
      sv->state = SV_RESTART_PENDING;
      sv->next_restart = now + 5;
      sv->restart_count++;

    }


};

struct timespec* sv_get_next_restart() {
  const service* current = sv_head;
  time_t now  = time(NULL);

  struct timespec* ts = calloc(1, sizeof(struct timespec));
  if(ts == NULL) return NULL;

  while(current) {
    if(current->state == SV_RESTART_PENDING) {

      ts->tv_sec = current->next_restart - now;

      return ts;
    }
    current = current -> next;
  }

  return NULL;
}

void sv_update_stable() { // if restart_count = 0 service is stable
  service* current = sv_head;
  time_t now = time(NULL);

  while(current) {
    if(now - current->start >= 60) {
      current->restart_count = 0;
      current->next_restart = 0;
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
    kill(sv->pid, SIGTERM);
    sv->state = SV_STOPPED;
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
  if(sv == NULL) return STATE_UNKNOW;
  switch (sv->state) {
    case SV_UP: return STATE_UP;
    case SV_RESTART_PENDING: return STATE_RESTART_PENDING;
    case SV_FAILED: return STATE_FAILED;
    case SV_STOPPED: return STATE_STOPPED;
    default: return STATE_UNKNOW;
  }
}
