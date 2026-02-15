#include <sys/mount.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "klog.h"

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


int main() {

  if(quick_mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC) == -1) {
    klog(FAIL, "failed to mount /dev/pts: %s\n", strerror(errno));
  } else klog(OK, "mounted /dev/pts");


  if(quick_mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    klog(FAIL, "failed to mount /dev/shm: %s\n", strerror(errno));
  } else klog(OK, "mount /dev/shm");


  if(quick_mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    klog(FAIL, "failed to mount /run: %s", strerror(errno));
  } else klog(OK, "mounted /run");

  if(quick_mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    klog(FAIL, "failed to mount /tmp: %s", strerror(errno));
  } else klog(OK, "mounted /tmp");


  if(quick_mount("cgroup2", "/sys/fs/cgroup", "cgroup2", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) {
    klog(FAIL, "failed to mount /sys/fs/cgroup: %s", strerror(errno));
  } else klog(OK, "mounted /sys/fs/cgroup");

  return 0;
}
