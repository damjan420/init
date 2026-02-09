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

  if(quick_mount(NULL, "/", NULL, MS_REMOUNT) == -1) {
    fprintf(stderr, "[ FAIL ] Mount rootfs as read-write: %s\n", strerror(errno));
  } else fprintf(stdout, "[ OK ] Mount rootfs as read-write\n");

  if(quick_mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) {
     fprintf(stderr, "[ FAIL ] Mount /proc: %s\n", strerror(errno));
  }
  else fprintf(stdout, "[ OK ] Mount proc\n");

  if(quick_mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1){
     fprintf(stderr, "[ FAIL ] Mount /sys: %s\n", strerror(errno));
  } else  fprintf(stdout, "[ OK ] Mount /sys\n");

  if(quick_mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID) == -1) {
     fprintf(stderr, "[ FAIL ] Mount /dev: %s\n", strerror(errno));
  } else fprintf(stdout, "[ OK ] Mount /dev\n");


  if(quick_mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /dev/pts: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /dev/pts\n");


  if(quick_mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /dev/shm: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /dev/shm\n");


  if(quick_mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /run: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /run\n");

  if(quick_mount("cgroup2", "/sys/fs/cgroup", "cgroup2", MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) {
    fprintf(stderr, "[ FAIL ] Mount /sys/fs/cgroup: %s\n", strerror(errno));
  } else fprintf(stderr, "[ OK ] Mount /sys/fs/cgroup\n");

  return 0;
}
