#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>

#include "klog.h"

void emergency_shell() {
  fprintf(stderr, "init: dropping emergency shell\n");
  pid_t pid = fork();
  if(pid == 0) {
    char* args[] = {"/bin/sh", NULL};
    execv(args[0], args);
    fprintf(stderr, "emergency shell failed: %s\n", strerror(errno));
  }
  waitpid(pid, NULL, 0);
}

void phase_one() {
  pid_t pid = getpid();
  if(pid != 1) return;

  int n = 0;

  if(mount(NULL, "/", NULL, MS_REMOUNT, NULL) == -1) {
    fprintf(stderr, "CRITICAL!!! failed to mount rootfs as read-write: %s\n", strerror(errno));
    emergency_shell();
  } else n++;

  if(mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1) {
     fprintf(stderr, "CRITICAL!!! failed to mount /proc: %s\n", strerror(errno));
     emergency_shell();
  } else n++;

  if(mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1){
     fprintf(stderr, "CRITICAL!!! failed to mount sys  /sys: %s\n", strerror(errno));
     emergency_shell();
  } else n++;

  if(mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, NULL) == -1) {
     fprintf(stderr, "CRITICAL!!! mount /dev: %s\n", strerror(errno));
     emergency_shell();
  }  else n++;

  set_loglevel(INFO +1);

  if(n == 4) {
    klog(OK, "mounted rootfs as read-write\n");
    klog(OK, "mounted pseudo filesystems /proc, /sys and dev");
  }

  klog(INFO, "starting parallel boot");

  pid = fork();

  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/mount_pfs",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service mount_pfs: %s", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/up_lo",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service up_lo: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/prep_cgroup",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service prep_cgroup: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/set_hostname",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service set_hostname: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/seed_entropy",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service seed_entropy: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/systohc",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service systohc: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/loadkeys_setfont",  NULL };
    execv(args[0], args);
    klog(FAIL, "failed to launch core service loadkeys: %s\n", strerror(errno));
    _exit(-1);
  }

  while (wait(NULL) > 0);
  klog(INFO, "made it here");
  set_loglevel(FAIL +1);

}
