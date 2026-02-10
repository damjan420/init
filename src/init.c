#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include "phase.h"
#include "init.h"

char* trim(char* str) {
  while(isspace((unsigned char)*str))str++;
  if(*str == 0) return str;
  char* end = str + strlen(str) -1;
  while(end > str && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return str;
}

service* parse_service(int sv_fd, const char* fname) {

  FILE* sv_fp = fdopen(sv_fd, "r");
  if(sv_fp == NULL) return NULL;

  service* sv = calloc(1, sizeof(service));

  size_t len;
  char* line = NULL;
  for(int ln=0;getline(&line, &len, sv_fp) > 0; ln++) {

     line[strcspn(line, "\r\n")] = '\0';
     if(strncmp(line, "#", 1) == 0 || strncmp(line, "\0", 1) == 0) continue;

     line = trim(line);

     char* eq = strchr(line, '=');
     if(eq == NULL) {
       printf("[ INFO ] Line %d in /etc/init.d/%s missing an equal characther '='. skipping line\n", ln, fname);
       continue;
     };

     *eq = '\0';
     const char* key = trim(line);

     const char* val = trim(eq + 1);

     if(strcmp(key, "exec") == 0) {
       sv->exec = strdup(val);

     }

  };
  fclose(sv_fp);
  if(line) free(line);
  else{
    free(sv);
    return NULL;
  }
  if(sv->exec == NULL) return NULL;

  return sv;
}


service* get_enabled_sv(){
  DIR* initd_dp = opendir("/etc/init.d/");
  if(initd_dp == NULL) return NULL;

  const struct dirent* entry;

  service* head = calloc(1, sizeof(service));
  bool head_set = false;

  while((entry = readdir(initd_dp)) != NULL) {
    const char* name = entry->d_name;
    size_t nlen = strlen(entry->d_name);

    if(strncmp(&(name[nlen-1]), "s", 1) && strncmp(&(name[nlen -1]), "v", 1) == 0 && entry->d_type == DT_REG) {

      int sv_fd = openat(dirfd(initd_dp), name, O_RDONLY);
      if(sv_fd == -1) fprintf(stderr, "[FAIL] Open file at /etc/init.d/%s: %s\n", name, strerror(errno));

      service* sv = parse_service(sv_fd, name);
      if(sv == NULL) {
        fprintf(stderr, "[ FAIL ] Unable to parse file etc/init.d/%s\n", name);
      }


      if(head_set == false) {
        head = sv;
        head_set = true;
      }
      else {
        sv->next = head;
        head = sv;
      }
      close(sv_fd);
    }
  }
  return head;
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

void execute_enabled(service* head) {
  service* current = head;
  while(current) {
    pid_t pid = fork();
    if(pid == 0) {
      fprintf(stdout, "[ INFO ] Executing %s\n", current->exec);
      execv_line(current->exec);
      fprintf(stderr, "[ FAIL ] Executing %s: %s\n", current->exec, strerror(errno));
      _exit(-1);
    }
    current->pid = pid;
    current = current->next;
  }
}

volatile sig_atomic_t shell_dead = true;
pid_t shell_pid = -1;

void handle_sigchld() {
  pid_t p;
  int save_errno = errno;
  while((p = waitpid(-1, NULL, WNOHANG)) > 0) {
    if(p == shell_pid) {
      shell_dead = true;
    }
    fprintf(stdout, "[ INFO ] reaped process: %d", p);
  }
  errno = save_errno;
}


void handler(int sig) {
  switch (sig) {
      case SIGCHLD:
        handle_sigchld();
        break;
       case SIGUSR1:
         phase_three();
         reboot(RB_POWER_OFF);
         break;
       case SIGUSR2:
         phase_three();
         reboot(RB_AUTOBOOT);
         break;
    }
}

int main() {

  phase_one();

  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  sigaction(SIGCHLD, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);

 service* head =  get_enabled_sv();

 execute_enabled(head);
  //TODO: make starting enabled services more robust, implement a ctl

  while(1){
    if(shell_dead == true) {
      shell_dead = false;

      pid_t pid = fork();
      shell_pid = pid;

      if(pid == 0) {
        char *args[] = { "/bin/sh",  NULL };

        execv(args[0], args);

        fprintf(stderr, "[ FAIL ] execv sh failed: %s\n", strerror(errno)); // TODO: start getty
        _exit(1);
      }
    }

    pause();

  }
}
