#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/random.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#include "klog.h"
#include "sv.h"

int count_processes() {
    int count = 0;
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (isdigit(entry->d_name[0])) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

void phase_three(int power_action) {

  sys_state = SYS_SHUTDOWN;

  set_loglevel(INFO +1);
  klog(INFO, "sending SIGTERM to all services");

  service* current = sv_head;
  while(current) {
    if(current->state == SV_UP) {
      current->state = SV_STOPPED;
      kill(current->pid, SIGTERM);
    }
    current = current->next;
  }

  klog(INFO, "starting grace period");


  for(int timeout = 30; timeout > 0; timeout--){
    while(waitpid(-1, NULL, WNOHANG) > 0);

    if(count_processes() <= 1) break;
    usleep(1000 * 100);
  }

  klog(INFO, "sending SIGKILL to all processes");
  kill(-1, SIGKILL);

  char next_seed[512];
  if(getrandom(&next_seed, 512, 0) < 0) {
    klog(FAIL, "failed to save random seed for next boot. getrandom() failed:  %s", strerror(errno));
  } else {
    int seed_fd = open("/var/lib/seed", O_WRONLY);
    if(seed_fd < 0) {
      klog(FAIL, "failed to save random seed for next boot. unable to open /var/lib/seed: %s", strerror(errno));
    } else {
      if(write(seed_fd, &next_seed, 512) < 0){
        klog(FAIL, "failed to save random seed for next boot. Unable to write /var/lib/seed: %s", strerror(errno));
      } else klog(OK, "saved random seed for next boot");
    }
    close(seed_fd);
  }

  sync();

 //dev/pts /dev/shm /run /sys/fs/cgroup

  if(umount("/run") < 0) {
    klog(FAIL, "failed to unmount /run: %s", strerror(errno));
  } else  klog(OK, "failed to unmount /run");

  if(umount("/dev/pts") < 0) {
    klog(FAIL, "failed to unmount  /dev/pts: %s", strerror(errno));
  } else  klog(OK, "unmounted /dev/pts\n");

  if(umount("/dev/shm") < 0) {
    klog(FAIL, "failed to unmount: %s", strerror(errno));
  } else  klog(OK, "unmounted /dev/shm");


  if(umount("/dev") < 0) {
    klog(FAIL, "failed to unmount /dev: %s", strerror(errno));
  } else  klog(OK, "unmounted /dev");

  if(umount("/sys/fs/cgroup") < 0) {
    klog(FAIL, "failed to unmount /sys/fs/cgroup: %s", strerror(errno));
  } else  klog(OK, "unmounted /sys/fs/cgroup");

  if(umount("/sys") < 0) {
    klog(FAIL, "failed to unmount /sys:  %s", strerror(errno));
  } else  klog(OK, "unmounted /sys");

  if(umount("/proc") < 0) {
    klog(FAIL, "failed to unmount proc: %s", strerror(errno));
  } else  klog(OK, "unmounted /proc");


  if(mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL) < 0) {
    klog(FAIL, "failed to mount rootfs as read-only: %s", strerror(errno));
  } else klog(OK, "mounted rootfs as read-only");

  if(power_action == 1) {
    reboot(RB_POWER_OFF);
  }
  else if(power_action == 2) {
    reboot(RB_AUTOBOOT);
  }

}
