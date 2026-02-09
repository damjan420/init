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
#include "phase.h"

volatile sig_atomic_t shell_dead = true;
pid_t shell_pid = -1;

void handle_sigchld() {
  pid_t p;
  while((p = waitpid(-1, NULL, WNOHANG)) > 0) {
    if(p == shell_pid) {
      shell_dead = true;
    }
    fprintf(stdout, "[ INFO ] reaped process: %d", p);
  }
}


void prepare_poweroff() {
  kill(-1, SIGTERM);
  sleep(2);
  kill(-1, SIGKILL);

  char next_seed[512];
  getrandom(&next_seed, 512, 0);

  int seed_fd = open("/var/lib/seed", O_WRONLY);
  write(seed_fd, &next_seed, 512);
  close(seed_fd);

  sync();

  umount("/dev");
  umount("/sys");
  umount("/proc");

  mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);

}

void handler(int sig) {
  switch (sig) {
      case SIGCHLD:
        handle_sigchld();
        break;
       case SIGUSR1:
         prepare_poweroff();
         reboot(RB_POWER_OFF);
         break;
       case SIGUSR2:
         prepare_poweroff();
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


  while(1){
    if(shell_dead == true) {
      shell_dead = false;

      pid_t pid = fork();
      shell_pid = pid;

      if(pid == 0) {
        char *args[] = { "/bin/sh",  NULL };

        execv(args[0], args);

        fprintf(stderr, "[ FAIL ] execv sh failed: %s\n", strerror(errno));
        _exit(1);
      }
    }

    pause();

  }
}
