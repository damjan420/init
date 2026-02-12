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
#include "sv.h"

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

char* sv_parse_name(const char* fname) {
  size_t flen = strlen(fname);
  char* name = strdup(fname);
  if(name == NULL) return NULL;
  name[(flen - 3)] = '\0';
  return name;
}

service* sv_parse(int sv_fd, const char* fname) {

  FILE* sv_fp = fdopen(sv_fd, "r");
  if(sv_fp == NULL) return NULL;

  service* sv = calloc(1, sizeof(service));

  size_t len;
  char* line = NULL;
  for(int ln=0; getline(&line, &len, sv_fp) > 0; ln++) {

     line[strcspn(line, "\r\n")] = '\0';
     if(strncmp(line, "#", 1) == 0 || strncmp(line, "\0", 1) == 0) continue;

     line = trim(line);

     char* eq = strchr(line, '=');
     if(eq == NULL) {
       fprintf(stdout, "[ INFO ] Line %d in service file %s missing an equal characther '='. skipping line\n", ln, fname);
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
         fprintf(stdout, "[ INFO ] Unknow restart option %s at line %d in service file %s. Assuming restart=never\n", val, ln, fname);
         sv->restart = SV_RS_NEVER;
       }

     }
     else {
       fprintf(stdout, "[ INFO ] Unknow key at line %d in service file %s. Skipping line\n", ln, key);
     }
  }
  fclose(sv_fp);
  if(line) free(line);

  else{
    free(sv);
    return NULL;
  }

  if(sv->exec == NULL) {
    fprintf(stderr, "[ FAIL ] Service file %s missing an exec field\n", fname);
    return NULL;
  }

  if(sv->restart == 0) {
    fprintf(stderr, "[ FAIL ] Service file %s missing a restart field\n ", fname);
  }

  sv->name = sv_parse_name(fname);

  return sv;
}


void sv_parse_enabled(){
  DIR* initd_dp = opendir("/etc/init.d/");
  if(initd_dp == NULL) return;

  const struct dirent* entry;

  while((entry = readdir(initd_dp)) != NULL) {
    const char* name = entry->d_name;
    size_t nlen = strlen(entry->d_name);

    if(strncmp(&(name[nlen-1]), "s", 1) && strncmp(&(name[nlen -1]), "v", 1) == 0 && entry->d_type == DT_REG) {

      int sv_fd = openat(dirfd(initd_dp), name, O_RDONLY);
      if(sv_fd == -1) fprintf(stderr, "[ FAIL ] Open file at /etc/init.d/%s: %s\n", name, strerror(errno));

      service* sv = sv_parse(sv_fd, name);
      if(sv == NULL) {
        fprintf(stderr, "[ FAIL ] Unable to parse file etc/init.d/%s\n", name);
      }


      if(sv_head == NULL) {
        sv_head = sv;
      }
      else {
        sv->next = sv_head;
        sv_head = sv;
      }
      close(sv_fd);
    }
  };
}

void sv_exec(service* sv) {
  if(sv == NULL) return;
  pid_t pid = fork();
  if(pid == 0) {
    fprintf(stdout, "[ INFO ] Executing service %s\n", sv->name);
    execv_line(sv->exec);
    fprintf(stderr, "[ FAIL ] Executing service %s: %s\n", sv->name, strerror(errno));
    _exit(-1);
  }
  sv->pid = pid;
  sv->state = SV_UP;
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


void sv_restart(time_t now) {
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
    fprintf(stdout, "[ INFO ] Service %s crashed 5 times in 25 seconds. Marking as FAILED\n", sv->name);
    sv->state = SV_FAILED;
    sv->next_restart = 0;
    return;
  }


    else if( (sv->restart == SV_RS_ALWAYS) || ( (sv->restart == SV_RS_ON_FAILURE) && ( (WIFEXITED(status) && WEXITSTATUS(status) != 0 ) || (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) ) ) ){

      fprintf(stdout, "[ INFO ] Service %s crashed. Next restart in 5 seconds\n", sv->name);
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
    if(current->state == SV_RESTART_PENDING || current->next_restart != 0) {

      ts->tv_sec = current->next_restart - now;

      return ts;
    }
    current = current -> next;
  }

  return NULL;
}
