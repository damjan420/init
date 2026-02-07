#include <stdlib.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/reboot.h>

int quick_mount(const char* src, const char* dest, const char* fs, uint64_t flags) {
  if(access(dest, F_OK) == -1) {
    mode_t mode = S_IRWXU | S_IRWXG | S_IROTH;
    mkdir(dest, mode);
  }
  if(mount(src, dest, fs, flags, NULL) != 0) {
    if(errno != EBUSY) {
      return -1;
    }
  }
  return 0;
};

char* read_hostname() {
  FILE* fp = fopen("/etc/hostname", "r");
  if(fp == NULL) {
    return NULL;
  }
  size_t len;
  char* line = NULL;
  if(getline(&line, &len, fp) == -1) {
    free(line);
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  line[strcspn(line, "\r\n")] = '\0';
  return line;
}

int set_lo() {

  int soc = socket(AF_INET, SOCK_DGRAM, 0);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", IFNAMSIZ);

  if(ioctl(soc, SIOCGIFFLAGS, &ifr) < 0) {
   close(soc);
   return -1;
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

  if(ioctl(soc, SIOCSIFFLAGS, &ifr) < 0) {
   close(soc);
   return -1;
  };

  close(soc);
  return 0;
}

int check_seed_entropy() {
  char buff[16];
  if(getrandom(&buff, sizeof(buff), GRND_NONBLOCK) < 0) {
    return -1;
  }
  return 0;
}

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
  pid_t pid = getpid();
  if(pid != 1) return 1;
  if(mount(NULL, "/", NULL, MS_REMOUNT, NULL) != 0) {

  }
  
  if(quick_mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) { 
     fprintf(stderr, "[ FAIL ] Failed to mount proc: %s\n", strerror(errno));
  }
  else fprintf(stdout, "[ OK ] Mounted proc\n");
  
  if(quick_mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1){
     fprintf(stderr, "[ FAIL ] Failed to mount sysfs: %s\n", strerror(errno));
  } else  fprintf(stdout, "[ OK ] Mounted sysfs\n");
  
  if(quick_mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID) == -1) {
     fprintf(stderr, "[FAIL] Failed to mount devtmpfs: %s\n", strerror(errno));
  } else fprintf(stdout, "[ OK ] Mounted devtmpfs\n");

  char* hostname = read_hostname();

  if(hostname == NULL) {
   fprintf(stderr, "[ FAIL ] Could not read hostname. Hostname not set\n");
  } else {
     size_t len = strlen(hostname);
    if(sethostname(hostname, len) != 0) {
      fprintf(stderr, "[ FAIL ] Failed to set hostname to %s: %s\n", hostname, strerror(errno));
    } else {
      fprintf(stdout, "[ OK ] Set hostname to %s\n", hostname);
    }
  }

  if(set_lo() == -1) {
    fprintf(stderr, "[ FAIL ] Loopback interface 'lo' is DOWN\n");
  } else {
     fprintf(stderr, "[ OK ] Loopback interface 'lo' is UP\n");
  }

  if(check_seed_entropy() == -1) {
     fprintf(stdout, "[ FAIL ] System random generator is DOWN: %s\n", strerror(errno));
  } else {
     fprintf(stdout, "[ OK ] System random generator is UP\n");
  }

  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  sigaction(SIGCHLD, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);


  while(1){
    if(shell_dead == true) {
      shell_dead = false;

      pid = fork();
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
