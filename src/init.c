#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ctl.h"
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


int handle_ctl_payload(ctl_payload* payload) {
  switch (payload->action) {
  case A_ENABLE:  return sv_enable(payload->sv_name, payload->euid);
  case A_DISABLE: return sv_disable(payload->sv_name, payload->euid);
  case A_START:   return sv_start(payload->sv_name, payload->euid);
  case A_STOP:    return sv_stop(payload->sv_name, payload->euid);
  default:        return ERR_UNSUPORTED;
  }
}

int main() {

  phase_one();

  sigset_t mask;
  sigemptyset(&mask);

  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);

  sigprocmask(SIG_BLOCK, &mask, NULL);

  int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  sv_enable("ntpd", 0);
  sv_parse_enabled();
  sv_exec_enabled();
  //TODO:  improve the ctl, improve logging

  int lfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  struct sockaddr_un addr  = { .sun_family = AF_UNIX, .sun_path = "/run/init.sock" };

  unlink("/run/init.sock");
  if( bind(lfd, (struct sockaddr * )&addr, sizeof(struct sockaddr_un)) < 0 ) fprintf(stderr, "[ FAIL ] Unable to bind the socket\n");
  if (listen(lfd, 5) == -1) {
   fprintf(stderr, "[ FAIL ] Unable to listen to Unix domain socket: %s\n", strerror(errno));
}

  while(1){

    struct pollfd fds[2];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;

    fds[1].fd = lfd;
    fds[1].events = POLLIN;

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
    int ret = ppoll(fds, 2, timeout, NULL);

    sv_update_stable();

    if( (ret > 0) && (fds[0].revents & POLLIN)) {

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

    else if((ret > 0) && (fds[1].revents & POLLIN)) {
      fflush(stdout);
      int cfd = accept(lfd, NULL, NULL);

      ctl_payload payload ;
      read(cfd, &payload, sizeof(ctl_payload));
      fprintf(stdout, "action: %d, sv: %s\n", payload.action, payload.sv_name);
      int err = handle_ctl_payload(&payload);
      write(cfd, &err, sizeof(int));
      close(cfd);

    }

    else sv_restart();

  }


}
