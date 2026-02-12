#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

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
#include <time.h>
#include <poll.h>
#include <sys/signalfd.h>

#include "phase.h"
#include "sv.h"

volatile sig_atomic_t shell_dead = true;
pid_t shell_pid = -1;

void handle_sigchld() {
  pid_t pid;
  int save_errno = errno;
  int status;
  while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    sv_schedule_restart(sv_find_by_pid(pid), status);
    if(pid == shell_pid) {
      shell_dead = true;
    }
    fprintf(stdout, "[ INFO ] reaped process: %d\n", pid);
  }
  errno = save_errno;
}


int main() {

  phase_one();

  /* struct sigaction sa; */
  /* sa.sa_handler = handler; */
  /* sa.sa_flags = SA_NOCLDSTOP; */

  /* sigaction(SIGCHLD, &sa, NULL); */
  /* sigaction(SIGUSR1, &sa, NULL); */
  /* sigaction(SIGUSR2, &sa, NULL); */

  sigset_t mask;
  sigemptyset(&mask);

  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);

  sigprocmask(SIG_BLOCK, &mask, NULL);

  int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

  sv_parse_enabled();
  sv_exec_enabled();
  //TODO: make starting enabled services more robust, implement a ctl

  while(1){

    struct pollfd fds[1];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;

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

    struct timespec* timeout = sv_get_next_restart();
    int ret = ppoll(fds, 1, timeout, NULL);

    if( (ret > 0) && (fds[0].events & POLLIN)) {
      struct signalfd_siginfo fdsi;
      read(fds[0].fd, &fdsi, sizeof(fdsi));
      if(fdsi.ssi_signo == SIGCHLD) {
        handle_sigchld();
      }
      else if(fdsi.ssi_signo == SIGUSR1) {
        phase_three(1);
      }
      else if(fdsi.ssi_signo == SIGUSR2) {
        phase_three(2);
      }
    }

    if(ret == 0) sv_restart(time(NULL));

  }


}
