#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>

void emergency_shell() {
  fprintf(stderr, "[ INFO ] Dropping emergency shell\n");
  pid_t pid = fork();
  if(pid == 0) {
    char* args[] = {"/bin/sh", NULL};
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Emergency shell: %s\n", strerror(errno));
  }
  waitpid(pid, NULL, 0);
}

void phase_one() {
  pid_t pid = getpid();
  if(pid != 1) return;


  if(mount(NULL, "/", NULL, MS_REMOUNT, NULL) == -1) {
    fprintf(stderr, "[ FAIL ] Mount rootfs as read-write: %s\n", strerror(errno));
    emergency_shell();

  } else fprintf(stdout, "[ OK ] Mount rootfs as read-write\n");

  if(mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1) {
     fprintf(stderr, "[ FAIL ] Mount /proc: %s\n", strerror(errno));
     emergency_shell();
  }
  else fprintf(stdout, "[ OK ] Mount /proc\n");

  if(mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1){
     fprintf(stderr, "[ FAIL ] Mount /sys: %s\n", strerror(errno));
     emergency_shell();
  } else  fprintf(stdout, "[ OK ] Mount /sys\n");

  if(mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, NULL) == -1) {
     fprintf(stderr, "[ FAIL ] Mount /dev: %s\n", strerror(errno));
     emergency_shell();
  } else fprintf(stdout, "[ OK ] Mount /dev\n");

  fprintf(stdout, "[ INFO ] Starting parallel boot\n");

  pid = fork();

  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/mount_pfs",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service mount_pfs: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/up_lo",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service up_lo: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/set_hostname",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service set_hostname: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/seed_entropy",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service seed_entropy: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/systohc",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service systohc: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/loadkeys_setfont",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service loadkeys: %s\n", strerror(errno));
    _exit(-1);
  }

  while (wait(NULL) > 0);
}
